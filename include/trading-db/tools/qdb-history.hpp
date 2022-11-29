#pragma once
#ifndef TRADING_DB_QDB_HISTORY_HPP_INCLUDED
#define TRADING_DB_QDB_HISTORY_HPP_INCLUDED

#include "../parts/qdb-common.hpp"
#include "../qdb.hpp"
#include "../utility/async-tasks.hpp"
#include <vector>
#include <set>
#include "ztime.hpp"

namespace trading_db {

	/** \brief Симулятор
	 */
	class QdbHistory {
	public:

		/** \brief Конфигурация тестера
		 */
		class Config {
		public:
			std::string					path_db;				/**< Путь к папке с БД котировок */
			std::vector<std::string>	symbols;				/**< Массив символов */
			uint64_t					start_date		= 0;	/**< Начальная дата теста (UTC, в секундах) */
			uint64_t					stop_date		= 0;	/**< Конечная дата теста (UTC, в секундах) */
			double						tick_period		= 1.0;	/**< Период тиков внутри бара (в секундах) */
			uint64_t					timeframe		= 60.0; /**< Таймфрейм исторических данных (в секундах) */

			std::vector<TimePeriod>		trade_period;		/**< Периоды торговли */

			/** \brief Установить даты симулятора
			 * Чтобы указать начальную дату, установите переменную stop = false
			 * Для конечной даты установить переменную stop = true
			 */
			inline void set_date(
					const bool	stop,
					const int	day,
					const int	month,
					const int	year,
					const int	hour	  = 0,
					const int	minute	  = 0,
					const int	second	  = 0) noexcept {
				if (stop) {
					stop_date = ztime::get_timestamp(day, month, year, hour, minute, second);
				} else {
					start_date = ztime::get_timestamp(day, month, year, hour, minute, second);
				}
			};

			inline void add_trade_period(const TimePeriod &user_period) noexcept {
				trade_period.push_back(user_period);
			}

			inline void add_trade_period(
					const TimePoint &user_start,
					const TimePoint &user_stop,
					const int32_t user_id = QDB_TIME_PERIOD_NO_ID) noexcept {
				add_trade_period(TimePeriod(user_start, user_stop, user_id));
			}

			std::function<void(
					const std::string &msg)>	on_msg		= nullptr;

			std::function<void(
					const size_t s_index)>		on_end_test_symbol	 = nullptr;

			std::function<void(
					const size_t i,
					const size_t n)>			on_end_test_thread	 = nullptr;

			std::function<void()>				on_end_test			= nullptr;

			std::function<void(
					const size_t s_index,
					const uint64_t t_ms)>		on_date_msg = nullptr;

			std::function<bool(
					const size_t s_index)>		on_symbol	= nullptr;

			std::function<void(
					const size_t				s_index,	// Номер символа
					const uint64_t				t_ms,		// Время тестера
					const std::set<int32_t>		&period_id, // Флаг периода теста (0 - если нет периода)
					const trading_db::Candle	&candle		// Данные бара
					)>							on_candle	 = nullptr;

			std::function<void(
					const size_t				s_index,	// Номер символа
					const uint64_t				t_ms,		// Время тестера
					const std::set<int32_t>		&period_id, // Флаг периода теста (0 - если нет периода)
					const trading_db::Tick		&tick		// Данные тика
					)>							on_tick	   = nullptr;

			std::function<void(
					const size_t				s_index,	// Номер символа
					const uint64_t				t_ms,		// Время тестера
					const std::set<int32_t>		&period_id	// Флаг периода теста (0 - если нет периода)
					)>							on_test	   = nullptr;
		}; // Config

		/** \brief Параметры сделки БО
		 */
		class TradeBoSignal {
		public:
			size_t		s_index		= 0;		/**< Индекс символа */
			uint64_t	t_ms		= 0;		/**< Время открытия сделки */
			double		delay		= 0.0;		/**< Задержка на вход в сделку */
			double		duration	= 0;		/**< Экспирация */
			bool		up			= false;	/**< Направление "на повышение"/"на понижение" */

			TradeBoSignal() {};

			TradeBoSignal(const size_t _s_index, const uint64_t _t_ms) :
				s_index(_s_index), t_ms(_t_ms) {};
		};

		/** \brief Результат сделки БО
		 */
		class TradeBoResult {
		public:
			double	open_price	= 0.0;			/**< Цена открытия */
			double	close_price = 0.0;			/**< Цена закрытия */
			double	send_date	= 0.0;			/**< Дата запроса */
			double	open_date	= 0.0;			/**< Дата открытия сделки */
			double	close_date	= 0.0;			/**< Дата закрытия сделки */
			bool	win			= false;		/**< Результат сделки "победа"/"поражение" */
			bool	ok			= false;		/**< Флаг инициализации результата сделки (для проверки достоверности данных) */
		};

	private:

		class InternalConfig {
		public:
			std::vector<uint64_t>			time_step_ms;
			std::vector<bool>				candle_flag;
			std::vector<std::set<int32_t>>	period_id;
			QDB_TIMEFRAMES					timeframe	= QDB_TIMEFRAMES::PERIOD_M1;

		} internal_config;

		std::vector<std::shared_ptr<QDB>>	symbols_db;
		utility::AsyncTasks					async_tasks;

		Config		config;
		Config		local_config;
		std::mutex	config_mutex;

		bool init(const Config &user_config) {

			const uint64_t tick_period_ms = (uint64_t)(user_config.tick_period * (double)ztime::MILLISECONDS_IN_SECOND + 0.5);
			const uint64_t timeframe_ms = user_config.timeframe * (uint64_t)ztime::MILLISECONDS_IN_SECOND;

			internal_config.timeframe = static_cast<QDB_TIMEFRAMES>(user_config.timeframe / 60);

			//{ Настариваем фильтр времени
			const uint64_t ms_day = ztime::SECONDS_IN_DAY * ztime::MILLISECONDS_IN_SECOND;
			for (uint64_t t_ms = 0; t_ms < ms_day; t_ms += tick_period_ms) {
				const uint64_t t = t_ms / ztime::MILLISECONDS_IN_SECOND;
				std::set<int32_t> period_id;
				bool is_trade_period = false;
				for (size_t j = 0; j < user_config.trade_period.size(); ++j) {
					const auto &p = user_config.trade_period[j];
					if (p.check_time(t)) {
						is_trade_period = true;
						period_id.insert(p.id);
					}
				}

				const bool is_candle = ((t_ms % timeframe_ms) == 0);
				if (is_trade_period || is_candle) {
					internal_config.time_step_ms.push_back(t_ms);
					internal_config.period_id.push_back(period_id);
					internal_config.candle_flag.push_back(is_candle);
				}
			}
			//}

			//{ Открываем все БД
			symbols_db.clear();
			for (size_t s = 0; s < user_config.symbols.size(); ++s) {
				symbols_db.push_back(std::make_shared<trading_db::QDB>());
				const std::string file_name = user_config.path_db + "\\" + user_config.symbols[s] + ".qdb";
				if (!symbols_db[s]->open(file_name, true)) {
					if (user_config.on_msg) user_config.on_msg("Database opening error! File name: " + file_name);
					return false;
				}
			}
			//}
			return true;
		}

	public:

		QdbHistory() {};
		~QdbHistory() {};

		inline Config get_config() noexcept {
			std::lock_guard<std::mutex> config_locker(config_mutex);
			return config;
		}

		inline void set_config(const Config &user_config) noexcept {
			std::lock_guard<std::mutex> config_locker(config_mutex);
			config = user_config;
		}

		inline Config get_local_config() noexcept {
			std::lock_guard<std::mutex> config_locker(config_mutex);
			return local_config;
		}

		bool check_trade_result(
				const TradeBoSignal &signal,	// Сигнал
				TradeBoResult		&result		// Результат сигнала
				) {

			const uint64_t duration_ms		= (uint64_t)(signal.duration * (double)ztime::MILLISECONDS_IN_SECOND);
			const uint64_t trade_delay_ms	= (uint64_t)(signal.delay * (double)ztime::MILLISECONDS_IN_SECOND);
			const uint64_t open_time_ms		= signal.t_ms + trade_delay_ms;
			const uint64_t close_time_ms	= open_time_ms + duration_ms;

			result.send_date = (double)signal.t_ms / (double)ztime::MILLISECONDS_IN_SECOND;
			result.open_date = (double)open_time_ms / (double)ztime::MILLISECONDS_IN_SECOND;
			result.close_date = (double)close_time_ms / (double)ztime::MILLISECONDS_IN_SECOND;
			result.ok = false;
			result.win = false;

			trading_db::Tick open_tick, close_tick;
			if (symbols_db[signal.s_index]->get_tick_ms(open_tick, open_time_ms) &&
				symbols_db[signal.s_index]->get_tick_ms(close_tick, close_time_ms)) {

				result.open_price = open_tick.bid;
				result.close_price = close_tick.bid;

				if (signal.up) {
					if (open_tick.bid < close_tick.bid) { // WIN
					   result.win = true;
					}
					result.ok = true;
					return true;
				} else {
					if (open_tick.bid > close_tick.bid) { // WIN
					   result.win = true;
					}
					result.ok = true;
					return true;
				}
			}
			return false;
		}

		void start() {
			// Получаем конфигурацию тестера
			std::unique_lock<std::mutex> config_locker(config_mutex);
			local_config = config;
			config_locker.unlock();
			// Инициализируем переменные
			init(local_config);
			// Предустановим константы
			const uint64_t start_date_ms =
				ztime::get_first_timestamp_day(local_config.start_date) *
				ztime::MILLISECONDS_IN_SECOND;
			const uint64_t stop_date_ms =
				ztime::get_first_timestamp_day(local_config.stop_date) *
				ztime::MILLISECONDS_IN_SECOND;

			// Количество потоков
			const size_t number_threads = std::thread::hardware_concurrency();

			std::mutex on_date_mutex;
			std::mutex f_mutex;

			for (size_t n = 0; n < number_threads; ++n) {
				async_tasks.create_task([&, n, number_threads]() {
					//{ Проходимся по всем символам
					for (size_t s = n; s < local_config.symbols.size(); s += number_threads) {
						// проверяем необходимость обработать символ
						if (local_config.on_symbol) {
							if (!local_config.on_symbol(s)) continue;
						}

						// Последнее время обновления индикаторов
						uint64_t last_update_time_ms = 0;

						const uint64_t date_step_ms = ztime::SECONDS_IN_DAY * ztime::MILLISECONDS_IN_SECOND;
						for (uint64_t
								date_ms = start_date_ms;
								date_ms <= stop_date_ms;
								date_ms += date_step_ms) {

							//{ Выводим сообщение о дате
							{
								std::lock_guard<std::mutex> locker(on_date_mutex);
								if (config.on_date_msg) config.on_date_msg(s, date_ms);
							}
							//} Выводим сообщение о дате

							for (size_t i = 0; i < internal_config.time_step_ms.size(); ++i) {
								uint64_t t_ms = date_ms + internal_config.time_step_ms[i];

								if (internal_config.candle_flag[i]) {
									//{ Вызываем on_candle
									const uint64_t t = t_ms / ztime::MILLISECONDS_IN_SECOND;
									const uint64_t timestamp_minute = ztime::get_first_timestamp_minute(t);
									const uint64_t timestamp_candle = timestamp_minute - ztime::SECONDS_IN_MINUTE;
									trading_db::Candle db_candle;
									if (symbols_db[s]->get_candle(db_candle, timestamp_candle, internal_config.timeframe)) {
										local_config.on_candle(s, t_ms, internal_config.period_id[i], db_candle);
										last_update_time_ms = (db_candle.timestamp + ztime::SECONDS_IN_MINUTE) * ztime::MILLISECONDS_IN_SECOND;
									}
									//} Вызываем on_candle
								} else {
									//{ Вызываем on_tick
									trading_db::Tick db_tick;
									if (symbols_db[s]->get_tick_ms(db_tick, t_ms)) {
										//{ Проверяем, что пришел новый тик нового бара
										if (db_tick.timestamp_ms > last_update_time_ms) {
											last_update_time_ms = db_tick.timestamp_ms;
											local_config.on_tick(s, t_ms, internal_config.period_id[i], db_tick);
										}
										//} Проверяем, что пришел новый тик нового бара
									}
									//} Вызываем on_tick
								}

								if (local_config.on_test && !internal_config.period_id[i].empty()) {
									local_config.on_test(s, t_ms, internal_config.period_id[i]);
								}
							} // for i
						} // for date_ms
						if (local_config.on_end_test_symbol) local_config.on_end_test_symbol(s);
					}; // for s
					if (local_config.on_end_test_thread) local_config.on_end_test_thread(n, number_threads);
					//} Проходимся по всем символам
				});
			}; // for n
			async_tasks.wait();
			if (local_config.on_end_test) local_config.on_end_test();
		}

	};
};

#endif // TRADING_DB_QDB_HISTORY_HPP_INCLUDED
