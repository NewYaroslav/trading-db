#pragma once
#ifndef TRADING_DB_QDB_PRICE_BUFFER_HPP_INCLUDED
#define TRADING_DB_QDB_PRICE_BUFFER_HPP_INCLUDED

#include <functional>
#include <map>
#include <array>
#include <vector>
#include "ztime.hpp"
#include "qdb-common.hpp"

namespace trading_db {

	/** \brief Буфер для хранения данных цен тиков и баров
	 */
	class QdbPriceBuffer {
	public:

		QdbPriceBuffer() {};

		~QdbPriceBuffer() {};

		class Config {
		public:
			int64_t tick_start	   = ztime::SECONDS_IN_DAY;		   /**< Количество секунд данных для загрузки в буфер до метки времени */
			int64_t tick_stop	   = ztime::SECONDS_IN_DAY;		   /**< Количество секунд данных для загрузки в буфер после метки времени */
			int64_t tick_deadtime  = ztime::SECONDS_IN_MINUTE;	   /**< Максимальное время отсутствия тиков */
			int64_t candle_start   = 10 * ztime::SECONDS_IN_DAY;
			int64_t candle_stop	   = 10 * ztime::SECONDS_IN_DAY;
			int64_t candle_deadtime	   = ztime::SECONDS_IN_MINUTE; /**< Максимальное время отсутствия баров */
			bool	candle_use_tick	   = true;
			QDB_PRICE_MODE candles_price_mode = QDB_PRICE_MODE::BID_PRICE;
		} config;

		std::function<std::map<uint64_t, ShortTick>(const uint64_t t)>				on_read_ticks = nullptr;
		std::function<std::array<Candle, ztime::MINUTES_IN_DAY>(const uint64_t t)>	on_read_candles = nullptr;

	private:

		template<typename Container, typename Key>
		typename Container::iterator lower_bound_dec(Container &container, const Key &key) {
			auto it = container.lower_bound(key);
			if (it == std::begin(container)) {
				if (it->first != key) it = std::end(container);
			} else {
				if (it->first != key) --it;
			}
			return it;
		}

		// данные тиков за час
		using ticks_hour = std::map<uint64_t, ShortTick>;
		// массив данных тиков
		std::map<uint64_t, ticks_hour> tick_buffer;

		void write_tick_buffer(const Tick &tick) noexcept {
			const uint64_t time_hour = ztime::get_first_timestamp_hour(tick.timestamp_ms/(uint64_t)ztime::MILLISECONDS_IN_SECOND);
			ShortTick short_tick;
			short_tick.ask = tick.ask;
			short_tick.bid = tick.bid;
			tick_buffer[time_hour][tick.timestamp_ms] = short_tick;
		}

		void read_tick_buffer(const uint64_t t_ms) noexcept {
			const uint64_t t = t_ms/(uint64_t)ztime::MILLISECONDS_IN_SECOND;
			const uint64_t start_time = t <= config.tick_start ? 0 : ztime::get_first_timestamp_hour(t - config.tick_start);
			const uint64_t stop_time = ztime::get_first_timestamp_hour(t + config.tick_stop);
			for (uint64_t rd_time = start_time; rd_time <= stop_time; rd_time += ztime::SECONDS_IN_HOUR) {
				if (tick_buffer.find(rd_time) == tick_buffer.end()) {
					tick_buffer[rd_time] = on_read_ticks(rd_time);
				}
			}
		}

		void read_next_tick_buffer(const uint64_t t_ms, const uint64_t t_ms_max) noexcept {
			const uint64_t t = t_ms/(uint64_t)ztime::MILLISECONDS_IN_SECOND;
			const uint64_t t_max = t_ms_max/(uint64_t)ztime::MILLISECONDS_IN_SECOND;

			const uint64_t start_time = ztime::get_first_timestamp_hour(t);
			uint64_t stop_time = ztime::get_first_timestamp_hour(t + config.tick_stop);

			bool has_last_tick = false;
			uint64_t rd_time = start_time;
			while(!false) {
                if (tick_buffer.find(rd_time) == tick_buffer.end()) {
                    auto data = on_read_ticks(rd_time);
                    tick_buffer[rd_time] = data;
                    if (!has_last_tick && !data.empty()) {
                        if (std::prev(data.end())->first > t_ms) {
                            has_last_tick = true;
                        }
                    }
                }
                rd_time += ztime::SECONDS_IN_HOUR;
                if (rd_time > stop_time && has_last_tick) break;
                if (rd_time > t_max) break;
			}
		}

		// очищаем тиковый буфре
		void erase_tick_buffer(const uint64_t t_ms) noexcept {
			const uint64_t t = t_ms/(uint64_t)ztime::MILLISECONDS_IN_SECOND;
			const uint64_t start_time =  t <= config.tick_start ? 0 : ztime::get_first_timestamp_hour(t - config.tick_start);
			const uint64_t stop_time = ztime::get_first_timestamp_hour(t + config.tick_stop);
			auto it = tick_buffer.begin();
			while (it != tick_buffer.end()) {
				if (it->first < start_time || it->first > stop_time) {
					it = tick_buffer.erase(it);
				} else ++it;
			}
		}

		void erase_next_tick_buffer(const uint64_t t_ms, const uint64_t t_ms_max) noexcept {
			const uint64_t t = t_ms/(uint64_t)ztime::MILLISECONDS_IN_SECOND;
			const uint64_t t_max = t_ms_max/(uint64_t)ztime::MILLISECONDS_IN_SECOND;
			const uint64_t start_time = ztime::get_first_timestamp_hour(t);
			const uint64_t stop_time = ztime::get_first_timestamp_hour(t_max);
			auto it = tick_buffer.begin();
			while (it != tick_buffer.end()) {
				if (it->first < start_time || it->first > stop_time) {
					it = tick_buffer.erase(it);
				} else ++it;
			}
		}

		bool get_tick_buffer(Tick &tick, const uint64_t t_ms) noexcept {
			const uint64_t time_hour = ztime::get_first_timestamp_hour(t_ms/(uint64_t)ztime::MILLISECONDS_IN_SECOND);
			auto it = tick_buffer.find(time_hour);
			if (it == tick_buffer.end()) return false;
			auto &buff = it->second;
			// находим последний тик
			auto it_tick = lower_bound_dec(buff, t_ms);

			if (it_tick == buff.end()) {
				// тика нет, ищем последний тик в предыдущих массивах

				// проверяем, есть ли данные в буфере
				if (it == tick_buffer.begin()) return false;
				auto it_prev = it;

				while (it_prev != tick_buffer.begin()) {
					// получаем предыдущий час тиков
					it_prev = std::prev(it_prev);
					// проверяем наличие данных в буфере
					if (!it_prev->second.empty()) break;
				}
				if (it_prev->second.empty()) return false;
				// находим последний тик предыдущего часа
				auto it_last = std::prev(it_prev->second.end());

				// проверяем мертвое время
				const int64_t deadtime = ((int64_t)t_ms - (int64_t)it_last->first)/(int64_t)ztime::MILLISECONDS_IN_SECOND;
				if (deadtime > config.tick_deadtime) return false;

				tick.ask = it_last->second.ask;
				tick.bid = it_last->second.bid;
				tick.timestamp_ms = it_last->first;
			} else {

				// проверяем мертвое время
				const int64_t deadtime = ((int64_t)t_ms - (int64_t)it_tick->first)/(int64_t)ztime::MILLISECONDS_IN_SECOND;
				if (deadtime > config.tick_deadtime) return false;

				tick.ask = it_tick->second.ask;
				tick.bid = it_tick->second.bid;
				tick.timestamp_ms = it_tick->first;
			}
			return true;
		}

		bool get_next_tick_buffer(Tick &tick, const uint64_t t_ms) noexcept {
			const uint64_t time_hour = ztime::get_first_timestamp_hour(t_ms/(uint64_t)ztime::MILLISECONDS_IN_SECOND);
			auto it = tick_buffer.find(time_hour);
			if (it == tick_buffer.end()) return false;
			auto &buff = it->second;
			// находим последний тик
			auto it_tick = buff.upper_bound(t_ms);

			if (it_tick == buff.end()) {
				// тика нет, ищем последний тик в следующих массивах
				// проверяем, есть ли данные в буфере
				if (it == std::prev(tick_buffer.end())) return false;
				auto it_prev = it;
				while (it_prev != tick_buffer.end()) {
					// получаем следующий час тиков
					it_prev = std::next(it_prev);
					// проверяем наличие данных в буфере
					if (!it_prev->second.empty()) break;
				}
				if (it_prev->second.empty()) return false;
				// находим первый тик следующего часа
				auto it_first = it_prev->second.begin();

				tick.ask = it_first->second.ask;
				tick.bid = it_first->second.bid;
				tick.timestamp_ms = it_first->first;
			} else {
				tick.ask = it_tick->second.ask;
				tick.bid = it_tick->second.bid;
				tick.timestamp_ms = it_tick->first;
			}
			return true;
		}

		bool get_ticks_buffer(std::vector<Tick> &ticks, const uint64_t t_ms_start, const uint64_t t_ms_stop) noexcept {
			const uint64_t start_time = ztime::get_first_timestamp_hour((t_ms_start/(uint64_t)ztime::MILLISECONDS_IN_SECOND));
			const uint64_t stop_time = ztime::get_first_timestamp_hour((t_ms_stop/(uint64_t)ztime::MILLISECONDS_IN_SECOND));

			auto it_start = tick_buffer.find(start_time);
			if (it_start == tick_buffer.end() || it_start->second.empty()) {

				// проверяем, есть ли данные в буфере
				if (it_start == tick_buffer.begin()) return false;
				auto it_prev = it_start;

				while (it_prev != tick_buffer.begin()) {
					// получаем предыдущий час тиков
					it_prev = std::prev(it_prev);
					// проверяем наличие данных в буфере
					if (!it_prev->second.empty()) break;
				}
				if (it_prev->second.empty()) return false;

				// находим последний тик предыдущего часа
				auto it_last = std::prev(it_prev->second.end());

				Tick tick;
				tick.ask = it_last->second.ask;
				tick.bid = it_last->second.bid;
				tick.timestamp_ms = it_last->first;
				ticks.push_back(tick);

				it_prev++;
				it_start = it_prev;
				if (it_start == tick_buffer.end()) return true;
			}

			auto it_stop = tick_buffer.find(stop_time);
			if (it_stop == tick_buffer.end() || it_stop->second.empty()) {

				// проверяем, есть ли данные в буфере
				if (it_stop == tick_buffer.begin()) {
					if (ticks.empty()) return false;
					return true;
				}
				auto it_prev = it_stop;

				while (it_prev != tick_buffer.begin()) {
					// получаем предыдущий час тиков
					it_prev = std::prev(it_prev);
					// проверяем наличие данных в буфере
					if (!it_prev->second.empty()) break;
				}

				if (it_prev->second.empty()) {
					if (ticks.empty()) return false;
					return true;
				}

				// находим последний тик предыдущего часа
				auto it_last = std::prev(it_prev->second.end());

				Tick tick;
				tick.ask = it_last->second.ask;
				tick.bid = it_last->second.bid;
				tick.timestamp_ms = it_last->first;

				if (!ticks.empty() && tick.timestamp_ms <= ticks.back().timestamp_ms) return true;
				it_stop = it_prev;
			}

			if (it_start == it_stop) {
				auto &buff = it_start->second;
				auto it_begin = lower_bound_dec(buff, t_ms_start);

				if (it_begin == buff.end()) {
					// данных нет в буфере, ищем буфером ниже

					// проверяем, есть ли данные в буфере
					if (it_start == tick_buffer.begin()) return false;
					auto it_prev = it_start;

					while (it_prev != tick_buffer.begin()) {
						// получаем предыдущий час тиков
						it_prev = std::prev(it_prev);
						// проверяем наличие данных в буфере
						if (!it_prev->second.empty()) break;
					}
					if (it_prev->second.empty()) return false;
					// находим последний тик предыдущего часа
					auto it_last = std::prev(it_prev->second.end());

					Tick tick;
					tick.ask = it_last->second.ask;
					tick.bid = it_last->second.bid;
					tick.timestamp_ms = it_last->first;
					ticks.push_back(tick);

					it_begin = buff.begin();
				}
				auto it_end = lower_bound_dec(buff, t_ms_stop);
				if (it_end == buff.end()) return false;

				++it_end;
				for (auto it_tick = it_begin; it_tick != it_end; ++it_tick) {
					Tick tick;
					tick.ask = it_tick->second.ask;
					tick.bid = it_tick->second.bid;
					tick.timestamp_ms = it_tick->first;
					ticks.push_back(tick);
				}
			} else {
				++it_stop;

				for (auto it = it_start; it != it_stop; ++it) {
					auto &buff = it->second;
					if (it == it_start) {
						auto it_begin = lower_bound_dec(buff, t_ms_start);
						if (it_begin == buff.end()) {
							// данных нет в буфере, ищем буфером ниже

							// проверяем, есть ли данные в буфере
							if (it_start == tick_buffer.begin()) return false;
							auto it_prev = it_start;

							while (it_prev != tick_buffer.begin()) {
								// получаем предыдущий час тиков
								it_prev = std::prev(it_prev);
								// проверяем наличие данных в буфере
								if (!it_prev->second.empty()) break;
							}
							if (it_prev->second.empty()) return false;
							// находим последний тик предыдущего часа
							auto it_last = std::prev(it_prev->second.end());

							Tick tick;
							tick.ask = it_last->second.ask;
							tick.bid = it_last->second.bid;
							tick.timestamp_ms = it_last->first;
							ticks.push_back(tick);
							continue;
						}

						for (auto it_tick = it_begin; it_tick != buff.end(); ++it_tick) {
							Tick tick;
							tick.ask = it_tick->second.ask;
							tick.bid = it_tick->second.bid;
							tick.timestamp_ms = it_tick->first;
							ticks.push_back(tick);
						}
					} else
					if (it == it_stop) {
						auto it_end = lower_bound_dec(buff, t_ms_stop);
						if (it_end == buff.end()) return false;
						++it_end;
						for (auto it_tick = buff.begin(); it_tick != it_end; ++it_tick) {
							Tick tick;
							tick.ask = it_tick->second.ask;
							tick.bid = it_tick->second.bid;
							tick.timestamp_ms = it_tick->first;
							ticks.push_back(tick);
						}
					} else {
						for (auto it_tick = buff.begin(); it_tick != buff.end(); ++it_tick) {
							Tick tick;
							tick.ask = it_tick->second.ask;
							tick.bid = it_tick->second.bid;
							tick.timestamp_ms = it_tick->first;
							ticks.push_back(tick);
						}
					}
				}
			}
			return true;
		}

		// bar data per day
		using candles_day = std::array<Candle, ztime::MINUTES_IN_DAY>;
		// array of bars/candle by day
		std::map<uint64_t, candles_day> candle_buffer;

		inline bool check_candle_buffer(const uint64_t t) noexcept {
			const uint64_t rd_time = ztime::get_first_timestamp_day(t);
			auto it = candle_buffer.find(rd_time);
			if (it == candle_buffer.end()) return false;
			return true;
		}

		void read_candle_buffer(const uint64_t t) noexcept {
			const uint64_t start_time = ztime::get_first_timestamp_day(t - config.candle_start);
			const uint64_t stop_time = ztime::get_first_timestamp_day(t + config.candle_stop);
			for (uint64_t rd_time = start_time; rd_time <= stop_time; rd_time += ztime::SECONDS_IN_DAY) {
				if (candle_buffer.find(rd_time) == candle_buffer.end()) {
					candle_buffer[rd_time] = on_read_candles(rd_time);
				}
			}
		}

		void erase_candle_buffer(const uint64_t t) noexcept {
			const uint64_t start_time = ztime::get_first_timestamp_day(t - config.candle_start);
			const uint64_t stop_time = ztime::get_first_timestamp_day(t + config.candle_stop);
			auto it = candle_buffer.begin();
			while (it != candle_buffer.end()) {
				if (it->first < start_time || it->first > stop_time) {
					it = candle_buffer.erase(it);
				} else ++it;
			}
		}

		bool get_candle_buffer(
				Candle &candle,
				const uint64_t t,
				const QDB_TIMEFRAMES p = QDB_TIMEFRAMES::PERIOD_M1,
				const QDB_CANDLE_MODE m = QDB_CANDLE_MODE::SRC_CANDLE) noexcept {

			if (m == QDB_CANDLE_MODE::SRC_CANDLE) {
				const uint64_t time_day = ztime::get_first_timestamp_day(t);
				auto it = candle_buffer.find(time_day);
				if (it == candle_buffer.end()) return false;
				const uint64_t minute_day = ztime::get_minute_day(t);
				switch (p) {
				case QDB_TIMEFRAMES::PERIOD_M1: {
						auto &c = it->second[minute_day];
						if (c.empty()) return false;
						candle = c;
					}
					break;
				case QDB_TIMEFRAMES::PERIOD_M5:
				case QDB_TIMEFRAMES::PERIOD_M15:
				case QDB_TIMEFRAMES::PERIOD_M30:
				case QDB_TIMEFRAMES::PERIOD_H1:
				case QDB_TIMEFRAMES::PERIOD_H4:
				case QDB_TIMEFRAMES::PERIOD_D1: {
						const uint64_t candle_period = static_cast<uint64_t>(p);
						const uint64_t start_minute_day = minute_day - minute_day % candle_period;
						// form a new bar
						Candle new_candle;
						new_candle.timestamp = start_minute_day * ztime::SECONDS_IN_MINUTE + time_day;
						for (uint64_t m = start_minute_day; m <= minute_day; ++m) {
							auto &c = it->second[m];
							if (c.empty()) continue;
							if (!new_candle.open) new_candle.open = c.open;

							if (!new_candle.high) new_candle.high = c.high;
							else if (c.high > new_candle.high) new_candle.high = c.high;

							if (!new_candle.low) new_candle.low = c.low;
							else if (c.low < new_candle.low) new_candle.low = c.low;

							new_candle.close = c.close;

							new_candle.volume += c.volume;
						}
						if (new_candle.empty()) return false;
						candle = new_candle;
					}
					break;
				};
			} else
			if (m == QDB_CANDLE_MODE::SRC_TICK) {
				const uint64_t candle_period = static_cast<uint64_t>(p);
				const uint64_t time_start = (t - (t % (candle_period * ztime::SECONDS_IN_MINUTE)));
				const uint64_t time_start_ms = time_start * ztime::MILLISECONDS_IN_SECOND;
				const uint64_t time_stop_ms = t * ztime::MILLISECONDS_IN_SECOND;
				// get an array of ticks to form an incomplete bar
				std::vector<Tick> ticks;
				if (!get_ticks_buffer(
					ticks,
					time_start_ms,
					time_stop_ms)) {
					return false;
				}
				if (config.candle_deadtime) {
					const int64_t deadtime = ((int64_t)time_stop_ms - (int64_t)ticks.back().timestamp_ms) / (int64_t)ztime::MILLISECONDS_IN_SECOND;
					if (deadtime > config.candle_deadtime) return false;
				}

				for (auto tick : ticks) {
					//std::cout << "tick " << tick.bid << " t " << ztime::get_str_date_time(tick.timestamp_ms/1000) << std::endl;
					double price = 0;
					switch (config.candles_price_mode) {
					case QDB_PRICE_MODE::BID_PRICE:
						price = tick.bid;
						break;
					case QDB_PRICE_MODE::ASK_PRICE:
						price = tick.ask;
						break;
					case QDB_PRICE_MODE::AVG_PRICE:
						price = ((tick.bid + tick.ask) / 2.0);
						break;
					}
					if (!candle.open) {
						candle.open = candle.high = candle.low = price;
						candle.timestamp = time_start;
					}
					if (price > candle.high) candle.high = price;
					if (price < candle.low) candle.low = price;
					candle.close = price;
				}
			}
			return true;
		}

	public:

		bool get_candle(
				Candle &candle,
				const uint64_t t,
				const QDB_TIMEFRAMES p = QDB_TIMEFRAMES::PERIOD_M1,
				const QDB_CANDLE_MODE m = QDB_CANDLE_MODE::SRC_CANDLE) noexcept {
			switch (m) {
			case QDB_CANDLE_MODE::SRC_CANDLE: {
					if (!check_candle_buffer(t)) {
						read_candle_buffer(t);
						erase_candle_buffer(t);
					}
				}
				break;
			case QDB_CANDLE_MODE::SRC_TICK: {
					const uint64_t t_ms = t * (uint64_t)ztime::MILLISECONDS_IN_SECOND;
					// ! Тут надо переделать реализацию
					Tick tick;
					if (!get_tick_buffer(tick, t_ms)) {
						erase_tick_buffer(t_ms);
						read_tick_buffer(t_ms);
					}
				}
				break;
			};
			return get_candle_buffer(candle, t, p, m);
		}

		bool get_tick(Tick &tick, const uint64_t t) noexcept {
			const uint64_t t_ms = t * (uint64_t)ztime::MILLISECONDS_IN_SECOND;
			if (!get_tick_buffer(tick, t_ms)) {
				erase_tick_buffer(t_ms);
				read_tick_buffer(t_ms);
				return get_tick_buffer(tick, t_ms);
			}
			return true;
		}

		bool get_tick_ms(Tick &tick, const uint64_t t_ms) noexcept {
			if (!get_tick_buffer(tick, t_ms)) {
                erase_tick_buffer(t_ms);
                read_tick_buffer(t_ms);
				return get_tick_buffer(tick, t_ms);
			}
			return true;
		}

		bool get_next_tick_ms(Tick &tick, const uint64_t t_ms, const uint64_t t_ms_max) noexcept {
			if (!get_next_tick_buffer(tick, t_ms)) {
                erase_tick_buffer(t_ms);
                read_next_tick_buffer(t_ms, t_ms_max);
                return get_next_tick_buffer(tick, t_ms);
			}
			return true;
		}

	};
};

#endif // TRADING_DB_QDB_PRICE_BUFFER_HPP_INCLUDED
