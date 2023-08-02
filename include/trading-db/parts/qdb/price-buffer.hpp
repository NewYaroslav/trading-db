#pragma once
#ifndef TRADING_DB_QDB_PRICE_BUFFER_HPP_INCLUDED
#define TRADING_DB_QDB_PRICE_BUFFER_HPP_INCLUDED

#include "enums.hpp"
#include "data-classes.hpp"
#include <functional>
#include <map>
#include <array>
#include <vector>
#include "ztime.hpp"

namespace trading_db {

	/** \brief Буфер для хранения данных цен тиков и баров
	 */
	class QdbPriceBuffer {
	public:

		QdbPriceBuffer() {};

		~QdbPriceBuffer() {};

		class Config {
		public:
			uint64_t tick_start	        = ztime::SEC_PER_HOUR;		    /**< Количество секунд данных для загрузки в буфер до метки времени */
			uint64_t tick_stop	        = ztime::SEC_PER_HOUR;		    /**< Количество секунд данных для загрузки в буфер после метки времени */
			uint64_t tick_deadtime      = ztime::SEC_PER_MIN;	        /**< Максимальное время отсутствия тиков */
			uint64_t candle_start	    = 10 * ztime::SEC_PER_DAY;
			uint64_t candle_stop	    = 10 * ztime::SEC_PER_DAY;
			uint64_t candle_deadtime    = ztime::SEC_PER_MIN;	        /**< Максимальное время отсутствия баров */
			bool	 candle_use_tick	= true;
			QDB_PRICE_MODE candles_price_mode = QDB_PRICE_MODE::BID_PRICE;
		} config;

		std::function<std::map<uint64_t, ShortTick>(const uint64_t t)>			on_read_ticks = nullptr;
		std::function<std::array<Candle, ztime::MIN_PER_DAY>(const uint64_t t)>	on_read_candles = nullptr;

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
			const uint64_t time_hour = ztime::start_of_hour_sec(tick.t_ms);
			ShortTick short_tick;
			short_tick.ask = tick.ask;
			short_tick.bid = tick.bid;
			tick_buffer[time_hour][tick.t_ms] = short_tick;
		}

		void read_tick_buffer(const uint64_t t_ms) noexcept {
			const uint64_t t = t_ms/(uint64_t)ztime::MS_PER_SEC;
			const uint64_t start_time = t <= config.tick_start ? 0 : ztime::start_of_hour(t - config.tick_start);
			const uint64_t stop_time = ztime::start_of_hour(t + config.tick_stop);
			for (uint64_t rd_time = start_time; rd_time <= stop_time; rd_time += ztime::SEC_PER_HOUR) {
				if (tick_buffer.find(rd_time) == tick_buffer.end()) {
					tick_buffer[rd_time] = on_read_ticks(rd_time);
				}
			}
		}

		void read_next_tick_buffer(const uint64_t t_ms, const uint64_t t_ms_max) noexcept {
			const uint64_t t = t_ms/(uint64_t)ztime::MS_PER_SEC;
			const uint64_t t_max = t_ms_max/(uint64_t)ztime::MS_PER_SEC;

			const uint64_t start_time = ztime::start_of_hour(t);
			uint64_t stop_time = ztime::start_of_hour(t + config.tick_stop);

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
				rd_time += ztime::SEC_PER_HOUR;
				if (rd_time > stop_time && has_last_tick) break;
				if (rd_time > t_max) break;
			}
		}

		// очищаем тиковый буфре
		void erase_tick_buffer(const uint64_t t_ms) noexcept {
			const uint64_t t = t_ms/(uint64_t)ztime::MS_PER_SEC;
			const uint64_t start_time =	 t <= config.tick_start ? 0 : ztime::start_of_hour(t - config.tick_start);
			const uint64_t stop_time = ztime::start_of_hour(t + config.tick_stop);
			auto it = tick_buffer.begin();
			while (it != tick_buffer.end()) {
				if (it->first < start_time || it->first > stop_time) {
					it = tick_buffer.erase(it);
				} else ++it;
			}
		}

		void erase_next_tick_buffer(const uint64_t t_ms, const uint64_t t_ms_max) noexcept {
			const uint64_t t = t_ms/(uint64_t)ztime::MS_PER_SEC;
			const uint64_t t_max = t_ms_max/(uint64_t)ztime::MS_PER_SEC;
			const uint64_t start_time = ztime::start_of_hour(t);
			const uint64_t stop_time = ztime::start_of_hour(t_max);
			auto it = tick_buffer.begin();
			while (it != tick_buffer.end()) {
				if (it->first < start_time || it->first > stop_time) {
					it = tick_buffer.erase(it);
				} else ++it;
			}
		}

		bool get_tick_buffer(Tick &tick, const uint64_t t_ms) noexcept {
			const uint64_t time_hour = ztime::start_of_hour_sec(t_ms);
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
				const int64_t deadtime = ztime::ms_to_sec((int64_t)t_ms - (int64_t)it_last->first);
				if (deadtime > (int64_t)config.tick_deadtime) return false;

				tick.ask = it_last->second.ask;
				tick.bid = it_last->second.bid;
				tick.t_ms = it_last->first;
			} else {

				// проверяем мертвое время
				const int64_t deadtime = ztime::ms_to_sec((int64_t)t_ms - (int64_t)it_tick->first);
				if (deadtime > (int64_t)config.tick_deadtime) return false;

				tick.ask = it_tick->second.ask;
				tick.bid = it_tick->second.bid;
				tick.t_ms = it_tick->first;
			}
			return true;
		}

		bool get_next_tick_buffer(Tick &tick, const uint64_t t_ms) noexcept {
			const uint64_t time_hour = ztime::start_of_hour_sec(t_ms);
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
				tick.t_ms = it_first->first;
			} else {
				tick.ask = it_tick->second.ask;
				tick.bid = it_tick->second.bid;
				tick.t_ms = it_tick->first;
			}
			return true;
		}

		bool get_ticks_buffer(std::vector<Tick> &ticks, const uint64_t t_ms_start, const uint64_t t_ms_stop) noexcept {
			const uint64_t start_time = ztime::start_of_hour_sec(t_ms_start);
			const uint64_t stop_time = ztime::start_of_hour_sec(t_ms_stop);

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
				tick.t_ms = it_last->first;
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
				tick.t_ms = it_last->first;

				if (!ticks.empty() && tick.t_ms <= ticks.back().t_ms) return true;
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
					tick.t_ms = it_last->first;
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
					tick.t_ms = it_tick->first;
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
							tick.t_ms = it_last->first;
							ticks.push_back(tick);
							continue;
						}

						for (auto it_tick = it_begin; it_tick != buff.end(); ++it_tick) {
							Tick tick;
							tick.ask = it_tick->second.ask;
							tick.bid = it_tick->second.bid;
							tick.t_ms = it_tick->first;
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
							tick.t_ms = it_tick->first;
							ticks.push_back(tick);
						}
					} else {
						for (auto it_tick = buff.begin(); it_tick != buff.end(); ++it_tick) {
							Tick tick;
							tick.ask = it_tick->second.ask;
							tick.bid = it_tick->second.bid;
							tick.t_ms = it_tick->first;
							ticks.push_back(tick);
						}
					}
				}
			}
			return true;
		}

		/*
		bool get_ticks_buffer_v2(std::vector<Tick> &ticks, const size_t num_ticks, const uint64_t t_ms, const uint64_t min_date, const uint64_t max_date) noexcept {
			uint64_t buffer_time = ztime::start_of_hour_sec(t_ms);
			if (buffer_time >= max_date) return false;
            if (buffer_time < min_date) return false;

            //{ Получаем данные из буфера
            auto it_buffer = tick_buffer.find(buffer_time);
            if (it_buffer == tick_buffer.end()) {
                tick_buffer[buffer_time] = on_read_ticks(buffer_time);
                it_buffer = tick_buffer.find(buffer_time);
                if (it_buffer == tick_buffer.end()) return false;
            }
            //}

            //{ чистим буферы после it_buffer
            const std::ptrdiff_t max_distance = std::max((std::ptrdiff_t)(config.tick_stop / ztime::SEC_PER_HOUR), std::ptrdiff_t(1));
            if (std::distance(it_buffer, tick_buffer.end()) > max_distance) {
                auto it_remove = std::next(it_buffer, max_distance);
                while (it_remove != tick_buffer.end()) {
                    it_remove = tick_buffer.erase(it_remove);
                }
            }
            //}

            //{ Загружаем буферы от t_ms
            const uint64_t last_time = std::prev(tick_buffer.end())->first + ztime::SEC_PER_HOUR;
            for (uint64_t read_time = last_time; read_time < max_date; read_time += ztime::SEC_PER_HOUR) {
                tick_buffer[buffer_time] = on_read_ticks(buffer_time);
            }
            //}

            //{ внутри первого элемента буфера
            while (!it_buffer->second.empty()) {
                auto &buffer = it_buffer->second;
                auto it_end = lower_bound_dec(buffer, t_ms);
                if (it_end == buffer.end()) break;
                // в буфере есть искомый тик
                if ((std::distance(buffer.begin(), it_end) + 1) >= num_ticks) {
                    auto it_begin = std::prev(it_end, num_ticks);
                    for (auto it = it_begin; it < it_end; ++it) {
                        Tick tick;
                        tick.ask = it->second.ask;
                        tick.bid = it->second.bid;
                        tick.timestamp_ms = it->first;
                        ticks.push_back(tick);
                    }
                    //{ чистим буферы после it_buffer
                    const std::ptrdiff_t max_distance = std::max((std::ptrdiff_t)(config.tick_start / ztime::SEC_PER_HOUR), 1);
                    if (std::distance(it_buffer, tick_buffer.end()) > max_distance) {
                        auto it_remove = std::next(it_buffer, max_distance);
                        while (it_remove != tick_buffer.end()) {
                            it_remove = tick_buffer.erase(it_remove);
                        }
                    }
                    //}

                    return true;
                } else {

                }
                break;
            }
            //}

            while (!false) {
                // если в буфере нет данных, грузим данные
                if (it_buffer == tick_buffer.end()) {
                    tick_buffer[buffer_time] = on_read_ticks(buffer_time);
                    it_buffer = tick_buffer.find(buffer_time);
                    if (it_buffer == tick_buffer.end()) return false;
                }
                // если данные пустые, смотрим предыдущие данные
                while (it_buffer->second.empty()) {
                    if (it_buffer == tick_buffer.begin()) {
                        // если попали в начало данных, грузим новые
                        buffer_time -= ztime::SEC_PER_HOUR;
                        if (buffer_time < min_date) return false;
                        tick_buffer[buffer_time] = on_read_ticks(buffer_time);
                        it_buffer = tick_buffer.find(buffer_time);
                    } else {
                        it_buffer = std::prev(it_buffer);
                        buffer_time = it_buffer->first;
                        if (buffer_time < min_date) return false;
                    }
                    continue;
                }
                // данные получены
                auto &buff = it_buffer->second;
                auto it_begin = lower_bound_dec(buff, t_ms_start);

                if (it_prev->second.size() >= num_ticks) {

                }

            }


			if (it_stop == tick_buffer.end() || it_stop->second.empty()) {
                // проверяем, есть ли данные в буфере
				if (it_stop == tick_buffer.begin()) return false;
				auto it_prev = it_stop;

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
			} else {
                auto &buff = it_start->second;
				auto it_begin = lower_bound_dec(buff, t_ms_start);

			}


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
		*/

		// bar data per day
		using candles_day = std::array<Candle, ztime::MIN_PER_DAY>;
		// array of bars/candle by day
		std::map<uint64_t, candles_day> candle_buffer;

		inline bool check_candle_buffer(const uint64_t t) noexcept {
			const uint64_t rd_time = ztime::start_of_day(t);
			auto it = candle_buffer.find(rd_time);
			if (it == candle_buffer.end()) return false;
			return true;
		}

		void read_candle_buffer(const uint64_t t) noexcept {
			const uint64_t start_time = ztime::start_of_day(t - config.candle_start);
			const uint64_t stop_time = ztime::start_of_day(t + config.candle_stop);
			for (uint64_t rd_time = start_time; rd_time <= stop_time; rd_time += ztime::SEC_PER_DAY) {
				if (candle_buffer.find(rd_time) == candle_buffer.end()) {
					candle_buffer[rd_time] = on_read_candles(rd_time);
				}
			}
		}

		void erase_candle_buffer(const uint64_t t) noexcept {
			const uint64_t start_time = ztime::start_of_day(t - config.candle_start);
			const uint64_t stop_time = ztime::start_of_day(t + config.candle_stop);
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
				const uint64_t time_day = ztime::start_of_day(t);
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
						new_candle.timestamp = start_minute_day * ztime::SEC_PER_MIN + time_day;
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
				const uint64_t time_start = (t - (t % (candle_period * ztime::SEC_PER_MIN)));
				const uint64_t time_start_ms = time_start * ztime::MS_PER_SEC;
				const uint64_t time_stop_ms = t * ztime::MS_PER_SEC;
				// get an array of ticks to form an incomplete bar
				std::vector<Tick> ticks;
				if (!get_ticks_buffer(
					ticks,
					time_start_ms,
					time_stop_ms)) {
					return false;
				}
				if (config.candle_deadtime) {
					const int64_t deadtime = ((int64_t)time_stop_ms - (int64_t)ticks.back().t_ms) / (int64_t)ztime::MS_PER_SEC;
					if (deadtime > (int64_t)config.candle_deadtime) return false;
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
					const uint64_t t_ms = t * (uint64_t)ztime::MS_PER_SEC;
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
			const uint64_t t_ms = t * (uint64_t)ztime::MS_PER_SEC;
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
