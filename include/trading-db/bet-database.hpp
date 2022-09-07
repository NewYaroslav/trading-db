#pragma once
#ifndef TRADING_DB_BET_DATABASE_HPP_INCLUDED
#define TRADING_DB_BET_DATABASE_HPP_INCLUDED

#if SQLITE_THREADSAFE != 1
#error "The project must be built for sqlite multithreading! Set the SQLITE_THREADSAFE=1"
#endif

#include "config.hpp"
#include "parts/bet-database-common.hpp"
#include "parts/bet-database-stats.hpp"
#include "parts/bet-database-meta-stats.hpp"

#include "utility/sqlite-func.hpp"
#include "utility/async-tasks.hpp"
#include "utility/safe-queue.hpp"
#include "utility/print.hpp"
#include "utility/files.hpp"

#include <ztime.hpp>
#include <mutex>
#include <atomic>
#include <future>
#include <vector>
#include <map>
#include <set>

namespace trading_db {

	/** \brief Класс базы данных ставок БО
	 */
	class BetDatabase {
	public:

		/** \brief Класс конфигурации БД
		 */
		class Config {
		public:
			const std::string title			= "trading_db::BetDatabase ";
			const std::string db_version	= "1.0";	/**< Версия БД */
			const std::string key_db_version	= "version";
			const std::string key_update_date	= "update-date";
			const std::string key_bet_uid		= "bet-id";
			double idle_time		= 15;	/**< Время бездействия записи */
			double meta_data_time	= 1;	/**< Время обновления мета данных */
			size_t busy_timeout		= 0;	/**< Время ожидания БД */
			size_t threshold_bets	= 1000; /**< Порог срабатывания по количеству сделок */
			std::atomic<bool> read_only = ATOMIC_VAR_INIT(false);
			std::atomic<bool> use_log	= ATOMIC_VAR_INIT(false);
		};

		Config config;	/**< Конфигурация БД */

		using ContractType = bet_database::ContractType;
		using BoType = bet_database::BoType;
		using BetStatus = bet_database::BetStatus;
		using BetData = bet_database::BetData;
#if(0)
		/// Направление ставки
		enum class ContractType {
			UNKNOWN_STATE = 0,
			BUY = 1,
			SELL = -1,
		};

		/// Тип ставки
		enum class BoType {
			SPRINT = 0,
			CLASSIC = 1,
		};

		/// Состояния сделки
		enum class BetStatus {
			UNKNOWN_STATE,			/**< Неопределенное состояние уже открытой сделки */
			OPENING_ERROR,			/**< Ошибка открытия */
			CHECK_ERROR,			/**< Ошибка проверки результата сделки */
			LOW_PAYMENT_ERROR,		/**< Низкий процент выплат */
			WAITING_COMPLETION,		/**< Ждем завершения сделки */
			WIN,					/**< Победа */
			LOSS,					/**< Убыток */
			STANDOFF,				/**< Ничья */
			UPDATE,					/**< Обновление состояния ставки */
			INCORRECT_PARAMETERS,	/**< Некорректные параметры ставки */
			AUTHORIZATION_ERROR		/**< Ошибкаавторизации */
		};

		/** \brief Класс ставки БО
		 */
		class BetData {
		public:
			int64_t uid			= 0;	/// ключ - уникальный ID сделки в БД
			int64_t broker_id	= 0;	/// уникальный номер сделки, который присваивает брокер
			int64_t open_date	= 0;	/// метка времени открытия сделки в миллисекундах
			int64_t close_date	= 0;	/// метка времени закрытия сделки в миллисекундах
			double open_price	= 0;	/// цена входа в сделку
			double close_price	= 0;	/// цена выхода из сделки

			double amount		= 0;	/// размер ставки
			double profit		= 0;	/// размер выплаты
			double payout		= 0;	/// процент выплат
			double winrate		= 0;	/// винрейт сигнала

			int64_t delay		= 0;	/// задержка на открытие ставки в миллисекундах
			int64_t ping		= 0;	/// пинг запроса на открытие ставки в миллисекундах

			uint32_t duration	= 0;	/// экспирация (длительность) бинарного опциона в секундах
			uint32_t step		= 0;	/// шаг систем риск менеджмента
			bool demo			= true;	/// флаг демо аккаунта
			bool last			= true;	/// флаг последней сделки - для подсчета винрейта в системах риск-менджента типа мартингейла и т.п.

			ContractType contract_type	= ContractType::UNKNOWN_STATE;	/// тип контракта, см.BetContractType
			BetStatus status			= BetStatus::UNKNOWN_STATE;		/// состояние сделки, см.BetStatus
			BoType type					= BoType::SPRINT;				/// тип бинарного опциона(SPRINT, CLASSIC и т.д.), см.BetType

			std::string symbol;		/// имя символа(валютная пара, акции, индекс и пр., например EURUSD)
			std::string broker;		/// имя брокера
			std::string currency;	/// валюта ставки
			std::string signal;		/// имя сигнала, стратегии или индикатора, короче имя источника сигнала
			std::string comment;	/// комментарий
			std::string user_data;	/// данные пользователя

			BetData() {};
		};
#endif
	private:
		std::string database_name;
		sqlite3 *sqlite_db = nullptr;
		// команды для транзакций
		utility::SqliteTransaction sqlite_transaction;
		// предкомпилированные команды
		utility::SqliteStmt stmt_replace_bet;
		utility::SqliteStmt stmt_replace_meta_data;
		utility::SqliteStmt stmt_get_meta_data;
		utility::SqliteStmt stmt_get_bet;
		utility::SqliteStmt stmt_get_all_bet;

		utility::SafeQueue<BetData> bet_safe_queue;

		// буфер для записи
		/*
		std::deque<BetData> write_buffer;
		int64_t write_last_bet_uid = 0;
		std::mutex write_buffer_mutex;
		*/
		// для бэкапа
		bool is_backup = ATOMIC_VAR_INIT(false);
		std::mutex backup_mutex;
		// флаг сброса
		std::atomic<bool> is_shutdown = ATOMIC_VAR_INIT(false);
		// уникальный номер сделки
		int64_t bet_uid = 0;
		std::mutex bet_uid_mutex;
		// флаг очистки буфера
		std::atomic<bool> is_flush = ATOMIC_VAR_INIT(false);
		// блокировка методов класса
		std::mutex method_mutex;

		utility::AsyncTasks async_tasks;

		/** \brief Класс для хранения мета данных
		 */
		class MetaData {
		public:
			std::string key;
			std::string value;

			MetaData() {};

			MetaData(const std::string &k, const std::string &v) : key(k), value(v) {};
		};

		std::atomic<uint64_t> last_update_date = ATOMIC_VAR_INIT(0);	/**< Последнее время обновления БД */

		inline void print_error(const std::string message, const int line) noexcept {
			if (config.use_log) {
				TRADING_DB_TICK_DB_PRINT
					<< config.title << "error in [file " << __FILE__
					<< ", line " << line
					<< "], message: " << message << std::endl;
			}
		}

		bool open_db(sqlite3 *&sqlite_db_ptr, const std::string &db_name, const bool readonly = false) {
			utility::create_directory(db_name, true);
			// открываем и возможно еще создаем таблицу
			int flags = readonly ?
				(SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX) :
				(SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX);
			if(sqlite3_open_v2(db_name.c_str(), &sqlite_db_ptr, flags, nullptr) != SQLITE_OK) {
				print_error("sqlite3_open_v2 error, db name " + db_name, __LINE__);
				return false;
			}
			sqlite3_busy_timeout(sqlite_db_ptr, config.busy_timeout);
			// создаем таблицу в базе данных, если она еще не создана
			const std::string create_bets_table_sql(
				"CREATE TABLE IF NOT EXISTS 'bets-data-v1' ("
				"uid			INTEGER NOT NULL,"
				"broker_id		INTEGER NOT NULL,"
				"open_date		INTEGER NOT NULL,"
				"close_date		INTEGER NOT NULL,"
				"open_price		REAL	NOT NULL,"
				"close_price	REAL	NOT NULL,"
				"amount			REAL	NOT NULL,"
				"profit			REAL	NOT NULL,"
				"payout			REAL	NOT NULL,"
				"winrate		REAL	NOT NULL,"
				"delay			INTEGER NOT NULL,"
				"ping			INTEGER NOT NULL,"
				"duration		INTEGER NOT NULL,"
				"step			INTEGER NOT NULL,"
				"demo			INTEGER NOT NULL,"
				"last			INTEGER NOT NULL,"
				"contract_type	INTEGER NOT NULL,"
				"status			INTEGER NOT NULL,"
				"type			INTEGER NOT NULL,"
				"symbol			TEXT	NOT NULL,"
				"broker			TEXT	NOT NULL,"
				"currency		TEXT	NOT NULL,"
				"signal			TEXT	NOT NULL,"
				"comment		TEXT	NOT NULL,"
				"user_data		TEXT	NOT NULL,"
				"PRIMARY KEY (open_date, uid))");

			const std::string create_meta_data_table_sql =
					"CREATE TABLE IF NOT EXISTS 'meta-data' ("
					"key				TEXT	PRIMARY KEY NOT NULL,"
					"value				TEXT				NOT NULL)";
			if (!utility::prepare(sqlite_db_ptr, create_bets_table_sql)) return false;
			if (!utility::prepare(sqlite_db_ptr, create_meta_data_table_sql)) return false;
			return true;
		}

		bool init_db(const std::string &db_name, const bool readonly = false) {
			config.read_only = readonly;
			if (!open_db(sqlite_db, db_name, readonly)) {
				sqlite3_close_v2(sqlite_db);
				sqlite_db = nullptr;
				return false;
			}
			if (!sqlite_transaction.init(sqlite_db) ||
				!stmt_replace_bet.init(sqlite_db, "INSERT OR REPLACE INTO 'bets-data-v1' ("
					"uid,"
					"broker_id,"
					"open_date,"
					"close_date,"
					"open_price,"
					"close_price,"
					"amount,"
					"profit,"
					"payout,"
					"winrate,"
					"delay,"
					"ping,"
					"duration,"
					"step,"
					"demo,"
					"last,"
					"contract_type,"
					"status,"
					"type,"
					"symbol,"
					"broker,"
					"currency,"
					"signal,"
					"comment,"
					"user_data)"
					"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)") ||
				!stmt_replace_meta_data.init(sqlite_db, "INSERT OR REPLACE INTO 'meta-data' (key, value) VALUES (?, ?)") ||
				!stmt_get_meta_data.init(sqlite_db, "SELECT * FROM 'meta-data' WHERE key == :x") ||
				!stmt_get_bet.init(sqlite_db, "SELECT * FROM 'bets-data-v1' WHERE open_date == :x AND uid == :y") ||
				!stmt_get_all_bet.init(sqlite_db, "SELECT * FROM 'bets-data-v1'")) {
				sqlite3_close_v2(sqlite_db);
				sqlite_db = nullptr;
				return false;
			}
			return true;
		}

		bool replace_db(
				const std::deque<BetData> &buffer,
				utility::SqliteTransaction &transaction,
				utility::SqliteStmt &stmt) {

			if (buffer.empty()) return true;
			if (!transaction.begin_transaction()) return false;
			sqlite3_reset(stmt.get());
			for (size_t i = 0; i < buffer.size(); ++i) {
				int index = 1;
				if (sqlite3_bind_int64(stmt.get(), index++, static_cast<sqlite3_int64>(buffer[i].uid)) != SQLITE_OK) {
					transaction.rollback();
					return false;
				}
				if (sqlite3_bind_int64(stmt.get(), index++, static_cast<sqlite3_int64>(buffer[i].broker_id)) != SQLITE_OK) {
					transaction.rollback();
					return false;
				}
				if (sqlite3_bind_int64(stmt.get(), index++, static_cast<sqlite3_int64>(buffer[i].open_date)) != SQLITE_OK) {
					transaction.rollback();
					return false;
				}
				if (sqlite3_bind_int64(stmt.get(), index++, static_cast<sqlite3_int64>(buffer[i].close_date)) != SQLITE_OK) {
					transaction.rollback();
					return false;
				}
				if (sqlite3_bind_double(stmt.get(), index++, static_cast<double>(buffer[i].open_price)) != SQLITE_OK) {
					transaction.rollback();
					return false;
				}
				if (sqlite3_bind_double(stmt.get(), index++, static_cast<double>(buffer[i].close_price)) != SQLITE_OK) {
					transaction.rollback();
					return false;
				}
				if (sqlite3_bind_double(stmt.get(), index++, static_cast<double>(buffer[i].amount)) != SQLITE_OK) {
					transaction.rollback();
					return false;
				}
				if (sqlite3_bind_double(stmt.get(), index++, static_cast<double>(buffer[i].profit)) != SQLITE_OK) {
					transaction.rollback();
					return false;
				}
				if (sqlite3_bind_double(stmt.get(), index++, static_cast<double>(buffer[i].payout)) != SQLITE_OK) {
					transaction.rollback();
					return false;
				}
				if (sqlite3_bind_double(stmt.get(), index++, static_cast<double>(buffer[i].winrate)) != SQLITE_OK) {
					transaction.rollback();
					return false;
				}
				if (sqlite3_bind_int(stmt.get(), index++, static_cast<int>(buffer[i].delay)) != SQLITE_OK) {
					transaction.rollback();
					return false;
				}
				if (sqlite3_bind_int(stmt.get(), index++, static_cast<int>(buffer[i].ping)) != SQLITE_OK) {
					transaction.rollback();
					return false;
				}
				if (sqlite3_bind_int(stmt.get(), index++, static_cast<int>(buffer[i].duration)) != SQLITE_OK) {
					transaction.rollback();
					return false;
				}
				if (sqlite3_bind_int(stmt.get(), index++, static_cast<int>(buffer[i].step)) != SQLITE_OK) {
					transaction.rollback();
					return false;
				}
				if (sqlite3_bind_int(stmt.get(), index++, static_cast<int>(buffer[i].demo)) != SQLITE_OK) {
					transaction.rollback();
					return false;
				}
				if (sqlite3_bind_int(stmt.get(), index++, static_cast<int>(buffer[i].last)) != SQLITE_OK) {
					transaction.rollback();
					return false;
				}
				if (sqlite3_bind_int(stmt.get(), index++, static_cast<int>(buffer[i].contract_type)) != SQLITE_OK) {
					transaction.rollback();
					return false;
				}
				if (sqlite3_bind_int(stmt.get(), index++, static_cast<int>(buffer[i].status)) != SQLITE_OK) {
					transaction.rollback();
					return false;
				}
				if (sqlite3_bind_int(stmt.get(), index++, static_cast<int>(buffer[i].type)) != SQLITE_OK) {
					transaction.rollback();
					return false;
				}
				if (sqlite3_bind_text(stmt.get(), index++, buffer[i].symbol.c_str(), -1, SQLITE_STATIC) != SQLITE_OK) {
					transaction.rollback();
					return false;
				}
				if (sqlite3_bind_text(stmt.get(), index++, buffer[i].broker.c_str(), -1, SQLITE_STATIC) != SQLITE_OK) {
					transaction.rollback();
					return false;
				}
				if (sqlite3_bind_text(stmt.get(), index++, buffer[i].currency.c_str(), -1, SQLITE_STATIC) != SQLITE_OK) {
					transaction.rollback();
					return false;
				}
				if (sqlite3_bind_text(stmt.get(), index++, buffer[i].signal.c_str(), -1, SQLITE_STATIC) != SQLITE_OK) {
					transaction.rollback();
					return false;
				}
				if (sqlite3_bind_text(stmt.get(), index++, buffer[i].comment.c_str(), -1, SQLITE_STATIC) != SQLITE_OK) {
					transaction.rollback();
					return false;
				}
				if (sqlite3_bind_text(stmt.get(), index++, buffer[i].user_data.c_str(), -1, SQLITE_STATIC) != SQLITE_OK) {
					transaction.rollback();
					return false;
				}

				const int err = sqlite3_step(stmt.get());
				sqlite3_reset(stmt.get());
				sqlite3_clear_bindings(stmt.get());
				if(err == SQLITE_DONE) {
					//...
				} else
				if(err == SQLITE_BUSY) {
					transaction.rollback();
					print_error("sqlite3_step return SQLITE_BUSY", __LINE__);
					return false;
				} else {
					transaction.rollback();
					print_error("sqlite3_step return " + std::to_string(err) + ", message " + std::string(sqlite3_errmsg(sqlite_db)), __LINE__);
					return false;
				}
			}
			if (!transaction.commit()) return false;
			return true;
		}

		bool replace_db(
				MetaData &pair,
				utility::SqliteTransaction &transaction,
				utility::SqliteStmt &stmt) noexcept {
			if (!transaction.begin_transaction()) return false;
			sqlite3_reset(stmt.get());
			if (sqlite3_bind_text(stmt.get(), 1, pair.key.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
				transaction.rollback();
				return false;
			}
			if (sqlite3_bind_text(stmt.get(), 2, pair.value.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
				transaction.rollback();
				return false;
			}
			int err = sqlite3_step(stmt.get());
			sqlite3_reset(stmt.get());
			sqlite3_clear_bindings(stmt.get());
			if(err == SQLITE_DONE) {
				//...
			} else
			if(err == SQLITE_BUSY) {
				// Если оператор не является COMMIT и встречается в явной транзакции, вам следует откатить транзакцию, прежде чем продолжить.
				transaction.rollback();
				print_error("sqlite3_step return SQLITE_BUSY", __LINE__);
				return false;
			} else {
				transaction.rollback();
				print_error(std::string(sqlite3_errmsg(sqlite_db)) + ", code " + std::to_string(err), __LINE__);
				return false;
			}
			if (!transaction.commit()) return false;
			return true;
		}

		template<class T>
		inline T get_bets_db(utility::SqliteStmt &stmt) noexcept {
			T buffer;
			int err = 0;

			while (true) {
				if ((err = sqlite3_reset(stmt.get())) != SQLITE_OK) {
					print_error("sqlite3_reset return code " + std::to_string(err), __LINE__);
					return T();
				}
				err = sqlite3_step(stmt.get());
				if(err == SQLITE_DONE) {
					sqlite3_reset(stmt.get());
					sqlite3_clear_bindings(stmt.get());
					return T();
				} else
				if(err == SQLITE_BUSY) {
					sqlite3_reset(stmt.get());
					sqlite3_clear_bindings(stmt.get());
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
					continue;
				} else
				if (err != SQLITE_ROW) {
					sqlite3_reset(stmt.get());
					sqlite3_clear_bindings(stmt.get());
					print_error("sqlite3_step return code " + std::to_string(err), __LINE__);
					return T();
				}

				err = 0;
				while (true) {
					typename T::value_type bet_data;

					size_t index = 0;
					bet_data.uid			= sqlite3_column_int64(stmt.get(), index++);
					bet_data.broker_id		= sqlite3_column_int64(stmt.get(), index++);

					bet_data.open_date		= sqlite3_column_double(stmt.get(), index++);
					bet_data.close_date		= sqlite3_column_double(stmt.get(), index++);

					bet_data.open_price		= sqlite3_column_double(stmt.get(), index++);
					bet_data.close_price	= sqlite3_column_double(stmt.get(), index++);

					bet_data.amount			= sqlite3_column_double(stmt.get(), index++);
					bet_data.profit			= sqlite3_column_double(stmt.get(), index++);
					bet_data.payout			= sqlite3_column_double(stmt.get(), index++);
					bet_data.winrate		= sqlite3_column_double(stmt.get(), index++);

					bet_data.delay			= sqlite3_column_int(stmt.get(), index++);
					bet_data.ping			= sqlite3_column_int(stmt.get(), index++);

					bet_data.duration		= static_cast<uint32_t>(sqlite3_column_int(stmt.get(), index++));
					bet_data.step			= static_cast<uint32_t>(sqlite3_column_int(stmt.get(), index++));

					bet_data.demo			= static_cast<bool>(sqlite3_column_int(stmt.get(), index++));
					bet_data.last			= static_cast<bool>(sqlite3_column_int(stmt.get(), index++));

					bet_data.contract_type	= static_cast<ContractType>(sqlite3_column_int(stmt.get(), index++));
					bet_data.status			= static_cast<BetStatus>(sqlite3_column_int(stmt.get(), index++));
					bet_data.type			= static_cast<BoType>(sqlite3_column_int(stmt.get(), index++));

					bet_data.symbol		= (const char *)sqlite3_column_text(stmt.get(), index++);
					bet_data.broker		= (const char *)sqlite3_column_text(stmt.get(), index++);
					bet_data.currency	= (const char *)sqlite3_column_text(stmt.get(), index++);
					bet_data.signal		= (const char *)sqlite3_column_text(stmt.get(), index++);
					bet_data.comment	= (const char *)sqlite3_column_text(stmt.get(), index++);
					bet_data.user_data	= (const char *)sqlite3_column_text(stmt.get(), index++);

					buffer.push_back(bet_data);
					err = sqlite3_step(stmt.get());
					if (err == SQLITE_ROW) {
						continue;
					} else
					if(err == SQLITE_DONE) {
						sqlite3_reset(stmt.get());
						sqlite3_clear_bindings(stmt.get());
						break;
					} else
					if(err == SQLITE_BUSY) {
						break;
					}
				}
				if(err == SQLITE_BUSY) {
					sqlite3_reset(stmt.get());
					sqlite3_clear_bindings(stmt.get());
					buffer.clear();
					print_error("sqlite3_step return SQLITE_BUSY", __LINE__);
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
					continue;
				}
				break;
			}
			return std::move(buffer);
		}

		inline MetaData get_meta_data_db(
				utility::SqliteStmt &stmt,
				const std::string &key) noexcept {
			MetaData meta_data;
			int err = 0;
			while (true) {
				if ((err = sqlite3_reset(stmt.get())) != SQLITE_OK) {
					print_error("sqlite3_reset return code " + std::to_string(err), __LINE__);
					return MetaData();
				}
				if (sqlite3_bind_text(stmt.get(), 1, key.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
					print_error("sqlite3_bind_text error", __LINE__);
					return MetaData();
				}
				err = sqlite3_step(stmt.get());
				if(err == SQLITE_DONE) {
					sqlite3_reset(stmt.get());
					sqlite3_clear_bindings(stmt.get());
					return MetaData();
				} else
				if(err == SQLITE_BUSY) {
					sqlite3_reset(stmt.get());
					sqlite3_clear_bindings(stmt.get());
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
					continue;
				} else
				if (err != SQLITE_ROW) {
					sqlite3_reset(stmt.get());
					sqlite3_clear_bindings(stmt.get());
					print_error("sqlite3_step return code " + std::to_string(err), __LINE__);
					return MetaData();
				}

				meta_data.key = (const char *)sqlite3_column_text(stmt.get(),0);
				meta_data.value = (const char *)sqlite3_column_text(stmt.get(),1);
				err = sqlite3_step(stmt.get());
				if (err == SQLITE_ROW) {
					break;
				} else
				if(err == SQLITE_DONE) {
					sqlite3_reset(stmt.get());
					sqlite3_clear_bindings(stmt.get());
					break;
				} else
				if(err == SQLITE_BUSY) {
					sqlite3_reset(stmt.get());
					sqlite3_clear_bindings(stmt.get());
					meta_data = MetaData();
					print_error("sqlite3_step return SQLITE_BUSY", __LINE__);
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
					continue;
				}
				break;
			}
			return std::move(meta_data);
		}

		// основная задача (запись данных в БД) для фонового процесса
		void main_task() {
			init_version();
			init_update_date();
			init_bet_uid();
			async_tasks.create_task([&]() {
				std::deque<BetData> buffer;
				ztime::Timer timer;
				ztime::Timer timer_meta_data;
				while (!is_shutdown) {
					// вычисляем время ожидания записи
					const size_t delay_ms =
						std::min(
							(size_t)(config.idle_time * ztime::MILLISECONDS_IN_SECOND),
							(size_t)(config.meta_data_time * ztime::MILLISECONDS_IN_SECOND)
						);
					bet_safe_queue.update(
							delay_ms,
							[&](const BetData &bet) {
								if (config.read_only) return;
								buffer.push_back(bet);
								// проверяем условие записи
								if (buffer.size() >= config.threshold_bets) {
									timer.reset();
									// запишем данные
									replace_db(buffer, sqlite_transaction, stmt_replace_bet);
									buffer.clear();
									// обновим мета-данные
									last_update_date = ztime::get_timestamp();
									MetaData meta_data_ut(config.key_update_date, std::to_string(last_update_date));
									int64_t update_bet_uid = 0;
									{
										std::lock_guard<std::mutex> lock(bet_uid_mutex);
										update_bet_uid = bet_uid;
									}
									MetaData meta_data_lbu(config.key_bet_uid, std::to_string(update_bet_uid));
									// запишем мета-данные
									replace_db(meta_data_ut, sqlite_transaction, stmt_replace_meta_data);
									replace_db(meta_data_lbu, sqlite_transaction, stmt_replace_meta_data);
									// очистим флаг flush
									is_flush = false;
								}
							},
							[&]() {
								// поступил сброс
								if (!buffer.empty()) {
									timer.reset();
									// запишем данные
									replace_db(buffer, sqlite_transaction, stmt_replace_bet);
									buffer.clear();
									// обновим мета-данные
									last_update_date = ztime::get_timestamp();
									MetaData meta_data_ut(config.key_update_date, std::to_string(last_update_date));
									int64_t update_bet_uid = 0;
									{
										std::lock_guard<std::mutex> lock(bet_uid_mutex);
										update_bet_uid = bet_uid;
									}
									MetaData meta_data_lbu(config.key_bet_uid, std::to_string(update_bet_uid));
									// запишем мета-данные
									replace_db(meta_data_ut, sqlite_transaction, stmt_replace_meta_data);
									replace_db(meta_data_lbu, sqlite_transaction, stmt_replace_meta_data);
								}
								// очистим флаг flush
								is_flush = false;
							},
							[&]() {
								// вышло время ожидания
								if (timer_meta_data.get_elapsed() >= config.meta_data_time) {
									timer_meta_data.reset();
									// читаем мета-данные
									const std::string value = get_meta_data_db(stmt_get_meta_data, config.key_update_date).value;
									try {
										if (!value.empty()) last_update_date = std::stoll(value);
									} catch(...) {
										last_update_date = ztime::get_timestamp();
									};
								}
								if (!buffer.empty() && timer.get_elapsed() >= config.idle_time) {
									timer.reset();
									// запишем данные
									replace_db(buffer, sqlite_transaction, stmt_replace_bet);
									buffer.clear();
									// обновим мета-данные
									last_update_date = ztime::get_timestamp();
									MetaData meta_data_ut(config.key_update_date, std::to_string(last_update_date));
									int64_t update_bet_uid = 0;
									{
										std::lock_guard<std::mutex> lock(bet_uid_mutex);
										update_bet_uid = bet_uid;
									}
									MetaData meta_data_lbu(config.key_bet_uid, std::to_string(update_bet_uid));
									// запишем мета-данные
									replace_db(meta_data_ut, sqlite_transaction, stmt_replace_meta_data);
									replace_db(meta_data_lbu, sqlite_transaction, stmt_replace_meta_data);
								}
								// очистим флаг flush
								is_flush = false;
							});
				} // while
				// очистим флаг flush
				is_flush = false;
			});
		}

		/** \brief Инициализация версии
		 */
		inline void init_version() noexcept {
			if (config.read_only) return;
			MetaData meta_data_dbv(config.key_db_version, config.db_version);
			replace_db(meta_data_dbv, sqlite_transaction, stmt_replace_meta_data);
		}

		/** \brief Инициализация времени
		 */
		inline void init_update_date() noexcept {
			const std::string value = get_meta_data_db(stmt_get_meta_data, config.key_update_date).value;
			try {
				if (!value.empty()) {
					last_update_date = std::stoll(value);
				} else {
					last_update_date = 0;
				}
			} catch(...) {
				last_update_date = 0;
			}
		}

		/** \brief Инициализация UID
		 */
		inline void init_bet_uid() noexcept {
			std::lock_guard<std::mutex> lock(bet_uid_mutex);
			const std::string value = get_meta_data_db(stmt_get_meta_data, config.key_bet_uid).value;
			try {
				if (!value.empty()) {
					bet_uid = std::stoll(value) + 1;
				} else {
					bet_uid = 1;
				}
			} catch(...) {
				bet_uid = 1;
			}
		}

		/** \brief Проверить инициализацию БД
		 * \return Вернет true если БД было инициализирована
		 */
		const bool check_init_db() noexcept {
			return (sqlite_db);
		}

		/** \brief Добавить в запрос аргумент для фильтрации
		 */
		inline void add_list_str_arg_req(
				const std::vector<std::string> &list_str,
				const std::string &column_str,
				const bool is_not,
				int &counter_args,
				std::string &request_str) noexcept {
			if (!list_str.empty()) {
				if (counter_args) request_str += " AND ";
				request_str += " " + column_str;
				if (is_not) request_str += " NOT IN (";
				else request_str += " IN (";
				for (size_t i = 0; i < list_str.size(); ++i) {
					if (i != 0) request_str += ",";
					request_str += "'" + list_str[i] + "'";
				} // for i
				request_str += ")";
				++counter_args;
			}
		};

		/** \brief Добавить в запрос аргумент для фильтрации
		 */
		template<class T>
		inline void add_list_num_arg_req(
				const T &list_num,
				const std::string &column_str,
				const bool is_not,
				int &counter_args,
				std::string &request_str) noexcept {
			if (!list_num.empty()) {
				if (counter_args) request_str += " AND ";
				request_str += " " + column_str;
				if (is_not) request_str += " NOT IN (";
				else request_str += " IN (";
				for (size_t i = 0; i < list_num.size(); ++i) {
					if (i != 0) request_str += ",";
					request_str += std::to_string(list_num[i]);
				} // for i
				request_str += ")";
				++counter_args;
			}
		};

		/** \brief Проверить список на наличие указанного числа
		 */
		template<class T1, class T2>
		inline bool check_list(
				const T1 &list_num,
				const bool is_not,
				const T2 num) noexcept {
			if (!list_num.empty()) {
				for (size_t i = 0; i < list_num.size(); ++i) {
					if (list_num[i] == num) return !is_not;
				}
				return is_not;
			}
			return true;
		}

	public:

		BetDatabase() {};

		/** \brief Конструктор хранилища тиков
		 * \param path		Путь к файлу
		 * \param readonly	Флаг 'только чтение'
		 */
		BetDatabase(const std::string &path, const bool readonly = false) {
			open(path, readonly);
		}

		~BetDatabase() {
			std::lock_guard<std::mutex> lock(method_mutex);
			// отправляем сигнал на очистку буфера
			is_flush = true;
			bet_safe_queue.reset();
			// ждем очистки
			while (is_flush) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			// отправляем сигнал на выход из задачи
			is_shutdown = true;
			bet_safe_queue.reset();
			async_tasks.wait();
			// закрываем соединение
			if (sqlite_db) sqlite3_close_v2(sqlite_db);
		}

		/** \brief Открыть БД
		 * \param path		Путь к файлу БД
		 * \param readonly	Флаг 'только чтение'
		 * \return Вернет true в случае успешной инициализации
		 */
		bool open(const std::string &path, const bool readonly = false) {
			std::lock_guard<std::mutex> lock(method_mutex);
			if (check_init_db()) return false;
			if (init_db(path, readonly)) {
				main_task();
				return true;
			}
			return false;
		}

		/** \brief Получить последнюю метку времени обновления БД
		 * \return Последняя метка времени обновления БД
		 */
		inline uint64_t get_last_update() noexcept {
			return last_update_date;
		}

		/** \brief Получить уникальный номер сделки
		 * \return Вернет уникальный номер сделки
		 */
		inline int64_t get_bet_uid() noexcept {
			std::lock_guard<std::mutex> lock(bet_uid_mutex);
			const int64_t temp = bet_uid;
			++bet_uid;
			return temp;
		}

		/** \brief Добавить или обновить ставку в БД
		 * Метод присвоит уникальный uid самостяотельно, если он не указан
		 * \param bet_data	Данные ставки
		 * \return Вернет true, если ставка была добавлена в очередь назапись в БД
		 */
		bool replace_bet(BetData &bet_data) noexcept {
			if (config.read_only) return false;
			std::lock_guard<std::mutex> lock(method_mutex);
			if (!check_init_db()) return false;
			// проверяем данные на валидность
			if (bet_data.open_date <= 0) return false;
			if (bet_data.uid <= 0) bet_data.uid = get_bet_uid();
			bet_safe_queue.enqueue(bet_data);
			return true;
		}

		/** \brief Очистить поток записи
		 * Данный блокирующий метод принуждает БД провести запись данных немедленно
		 */
		inline void flush() noexcept {
			if (config.read_only) return;
			std::lock_guard<std::mutex> lock(method_mutex);
			is_flush = true;
			bet_safe_queue.reset();
			while (is_flush && !is_shutdown) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}

		/** \brief Параметры запроса для выгрузки ставок из БД
		 */
		class RequestConfig {
		public:
			int64_t start_date	 = 0;					/// Начальная дата запроса, в мс
			int64_t stop_date	 = 0;					/// Конечная дата запроса, в мс
			std::vector<std::string>	brokers;		/// Список брокеров. Если пусто, то не фильтруется
			std::vector<std::string>	no_brokers;		/// Список запрещенных брокеров. Если пусто, то не фильтруется
			std::vector<std::string>	symbols;		/// Список символов. Если пусто, то не фильтруется
			std::vector<std::string>	no_symbols;		/// Список запрещенных символов. Если пусто, то не фильтруется
			std::vector<std::string>	signals;		/// Список сигналов. Если пусто, то не фильтруется
			std::vector<std::string>	no_signals;		/// Список запрещенных сигналов. Если пусто, то не фильтруется
			std::vector<std::string>	currency;		/// Список валют счета. Если пусто, то не фильтруется
			std::vector<std::string>	no_currency;	/// Список запрещенных валют счета. Если пусто, то не фильтруется
			std::vector<uint32_t>		durations;		/// Список экспираций. Если пусто, то не фильтруется
			std::vector<uint32_t>		no_durations;	/// Список запрещенных экспираций. Если пусто, то не фильтруется
			std::vector<uint32_t>		hours;			/// Список торговых часов. Если пусто, то не фильтруется
			std::vector<uint32_t>		no_hours;		/// Список запрещенных торговых часов. Если пусто, то не фильтруется
			std::vector<uint32_t>		weekday;		/// Список торговых дней недели. Если пусто, то не фильтруется
			std::vector<uint32_t>		no_weekday;		/// Список запрещенных торговых дней недели. Если пусто, то не фильтруется
			uint32_t start_time	 = 0;					/// Начальное время торговли
			uint32_t stop_time	 = 0;					/// Конечное время торговли

			double min_amount	 = 0;					/// Минимальный размер ставки
			double max_amount	 = 0;					/// Максимальный размер ставки
			double min_payout	 = 0;					/// Минимальный процент выплат
			double max_payout	 = 0;					/// Максимальный процент выплат
			int64_t min_ping	 = 0;					/// Минимальный пинг запроса на открытие ставки в миллисекундах
			int64_t max_ping	 = 0;					/// Максимальный пинг запроса на открытие ставки в миллисекундах

			bool only_last		 = false;				/// Использовать только последние сделки в цепочке (актуально для мартингейла)
			bool only_result	 = false;				/// Использовать только сделки с результатом
			bool use_buy		 = true;				/// Использовать сделки BUY
			bool use_sell		 = true;				/// Использовать сделки SELL
			bool use_demo		 = true;				/// Использовать сделки на DEMO
			bool use_real		 = true;				/// Использовать сделки на REAL

		};

		/** \brief Получить данные из БД
		 * \param request	Структура запроса, которая определяет фильтрацию данных
		 * \return Вернет массив ставок, если есть. Если массив пустой, то данных удовлетворяющих запросу нет
		 */
		template<class T>
		inline T get_bets(const RequestConfig &request) noexcept {
			{
				std::lock_guard<std::mutex> lock(method_mutex);
				if (!check_init_db()) return T();
			}
			// формируем запрос
			std::string request_str("SELECT * FROM 'bets-data-v1' WHERE");
			int counter_args = 0; //  счетчик аргументов
			if (request.start_date) {
				request_str += " open_date >= " + std::to_string(request.start_date);
				++counter_args;
			}
			if (request.stop_date) {
				if (counter_args) request_str += " AND ";
				request_str += " open_date <= " + std::to_string(request.stop_date);
				++counter_args;
			}

			add_list_str_arg_req(request.brokers, "broker", false, counter_args, request_str);
			add_list_str_arg_req(request.no_brokers, "broker", true, counter_args, request_str);
			add_list_str_arg_req(request.symbols, "symbol", false, counter_args, request_str);
			add_list_str_arg_req(request.no_symbols, "symbol", true, counter_args, request_str);
			add_list_str_arg_req(request.signals, "signal", false, counter_args, request_str);
			add_list_str_arg_req(request.no_signals, "signal", true, counter_args, request_str);
			add_list_str_arg_req(request.currency, "currency", false, counter_args, request_str);
			add_list_str_arg_req(request.no_currency, "currency", true, counter_args, request_str);
			add_list_num_arg_req(request.durations, "duration", false, counter_args, request_str);
			add_list_num_arg_req(request.no_durations, "duration", true, counter_args, request_str);

			if (counter_args == 0) request_str += " open_date >= 0";
			request_str += " ORDER BY open_date ASC";

			// подготавливаем запрос
			utility::SqliteStmt stmt;
			stmt.init(sqlite_db, request_str);
			// получаем данные
			T buffer(get_bets_db<T>(stmt));
			// проводим оставшуюся фильтрацию
			size_t index = 0;
			while (index < buffer.size()) {
				// получаем метку времени
				const uint64_t timestamp = buffer[index].open_date / ztime::MILLISECONDS_IN_SECOND;
				const uint32_t hour = ztime::get_hour_day(timestamp);
				const uint32_t weekday = ztime::get_weekday(timestamp);

				if (!check_list(request.weekday, false, weekday) ||		// фильтруем по дням недели
					!check_list(request.no_weekday, true, weekday) ||	// фильтруем по дням недели
					!check_list(request.hours, false, hour) ||			// фильтруем по часам
					!check_list(request.no_hours, true, hour) ||		// фильтруем по часам
					!check_list(request.durations, false, buffer[index].duration) ||	// фильтруем по часам
					!check_list(request.no_durations, true, buffer[index].duration)) {	// фильтруем по часам
					buffer.erase(buffer.begin() + index);
					continue;
				}

				if (request.start_time || request.stop_time) {
					const uint32_t second_day = ztime::get_second_day(timestamp);
					if ((request.start_time && request.start_time > second_day) ||
					   (request.stop_time && request.stop_time < second_day)){
						buffer.erase(buffer.begin() + index);
						continue;
					}
				}

				if (request.min_amount || request.max_amount) {
					if ((request.min_amount && request.min_amount > buffer[index].amount) ||
					   (request.max_amount && request.max_amount < buffer[index].amount)){
						buffer.erase(buffer.begin() + index);
						continue;
					}
				}

				if (request.min_payout || request.max_payout) {
					if ((request.min_payout && request.min_payout > buffer[index].payout) ||
					   (request.max_payout && request.max_payout < buffer[index].payout)){
						buffer.erase(buffer.begin() + index);
						continue;
					}
				}

				if (request.min_ping || request.max_ping) {
					if ((request.min_ping && request.min_ping > buffer[index].ping) ||
					   (request.max_ping && request.max_ping < buffer[index].ping)){
						buffer.erase(buffer.begin() + index);
						continue;
					}
				}

				if (request.only_last) {
					if (!buffer[index].last) {
						buffer.erase(buffer.begin() + index);
						continue;
					}
				}

				if (request.only_result) {
					if (buffer[index].status != BetStatus::WIN &&
						buffer[index].status != BetStatus::LOSS &&
						buffer[index].status != BetStatus::STANDOFF) {
						buffer.erase(buffer.begin() + index);
						continue;
					}
				}

				if (request.only_result) {
					if (buffer[index].status != BetStatus::WIN &&
						buffer[index].status != BetStatus::LOSS &&
						buffer[index].status != BetStatus::STANDOFF) {
						buffer.erase(buffer.begin() + index);
						continue;
					}
				}

				if ((!request.use_buy && buffer[index].contract_type == ContractType::BUY) ||
				   (!request.use_sell && buffer[index].contract_type == ContractType::SELL)) {
					buffer.erase(buffer.begin() + index);
					continue;
				}

				if ((!request.use_real && !buffer[index].demo) ||
				   (!request.use_demo && buffer[index].demo)) {
					buffer.erase(buffer.begin() + index);
					continue;
				}
				++index;
			}
			return std::move(buffer);
		}

		/** \brief Создать бэкап
		 * \param path	Путь к бэкапу базы данных
		 * \return Вернет true, если бэкап начался
		 */
		bool backup(
				const std::string &path,
				const std::function<void(const std::string &path, const bool is_error)> callback) {
			{
				std::lock_guard<std::mutex> lock(backup_mutex);
				if (is_backup) return false;
				is_backup = true;
			}
			async_tasks.create_task([&, path]() {
				if (!utility::backup_form_db(path, this->sqlite_db)) {
					callback(path, true);
					print_error("backup return false", __LINE__);
				} else {
					callback(path, false);
				}
				{
					std::lock_guard<std::mutex> lock(backup_mutex);
					is_backup = false;
				}
			});
			return true;
		}

		/** \brief Очистить все данные
		 */
		inline bool remove_all() {
			std::lock_guard<std::mutex> lock(method_mutex);
			if (!check_init_db()) return false;
			return utility::prepare(sqlite_db, "DELETE FROM 'bets-data-v1'");
		}

		/** \brief Удалить сделку
		 * \param BetData Заполненная структура сделки для удаления
		 */
		inline bool remove_bet(const BetData &bet_data) noexcept {
			std::lock_guard<std::mutex> lock(method_mutex);
			if (!check_init_db()) return false;
			return utility::prepare(sqlite_db,
				"DELETE FROM 'bets-data-v1' WHERE open_date == " +
				std::to_string(bet_data.open_date) +
				" AND uid == " +
				std::to_string(bet_data.uid));
		}

		/** \brief Удалить сделку по UID
		 * \param uid	Уникальный номер сделки в БД
		 */
		inline bool remove_bet(const int64_t uid) noexcept {
			std::lock_guard<std::mutex> lock(method_mutex);
			if (!check_init_db()) return false;
			return utility::prepare(sqlite_db,
				"DELETE FROM 'bets-data-v1' WHERE uid == " +
				std::to_string(uid));
		}

		/** \brief Удалить значения по ключам
		 * \param keys	Массив ключей элементов списка
		 */
		template<class T>
		inline bool remove_bets(const T &keys) noexcept {
			std::lock_guard<std::mutex> lock(method_mutex);
			if (!check_init_db()) return false;
			if (keys.empty()) return false;
			std::string message("DELETE FROM 'bets-data-v1' WHERE key IN (");
			for (auto it = keys.begin(); it != keys.end(); ++it) {
				if (it != keys.begin()) message += ",";
				message += *it;
			}
			message += ")";
			return utility::prepare(sqlite_db, message);
		}

		//----------------------------------------------------------------------
#if (0)
		/** \brief Статистика ставок
		 */
		class BetStats {
		public:

			class WinrateStats {
			public:
				uint64_t	wins		= 0;
				uint64_t	losses		= 0;
				uint64_t	standoffs	= 0;
				uint64_t	deals		= 0;
				double		winrate		= 0;

				WinrateStats() {};

				WinrateStats(
					const uint64_t w,
					const uint64_t l,
					const uint64_t s) : wins(w), losses(l), standoffs(s) {};

				inline void win() noexcept {
					++wins;
				}

				inline void loss() noexcept {
					++losses;
				}

				inline void standoff() noexcept {
					++standoffs;
				}

				inline void calc() noexcept {
					deals = wins + losses + standoffs;
					winrate = deals == 0 ? 0.0 : (double)wins / (double)deals;
				}

				inline void clear() noexcept {
					wins		= 0;
					losses		= 0;
					standoffs	= 0;
					deals		= 0;
					winrate		= 0;
				}
			};

			class ChartData {
			public:
				std::vector<double>			y_data;
				std::vector<double>			x_data;
				std::vector<std::string>	x_label;

				inline void clear() noexcept {
					y_data.clear();
					x_data.clear();
					x_label.clear();
				}
			};

		private:

			// статистика по парам
			std::map<std::string, WinrateStats> stats_symbol;

			// статистика по периодам
			std::map<uint64_t, WinrateStats> stats_year;
			std::map<uint32_t, WinrateStats> stats_month;
			std::map<uint32_t, WinrateStats> stats_day_month;
			std::map<uint32_t, WinrateStats> stats_hour_day;
			std::map<uint32_t, WinrateStats> stats_minute_day;
			std::map<uint32_t, WinrateStats> stats_week_day;
			std::map<uint32_t, WinrateStats> stats_expiration;

			// статистика по кол-ву сигналов
			std::map<uint32_t, WinrateStats> stats_counter_bet;
			WinrateStats stats_temp_counter_bet;
			uint64_t counter_bet_timestamp = 0;
			uint32_t counter_bet = 1;

			// для построения графика баланса
			std::map<uint64_t, double> temp_balance;

		public:

			BetStats() {
				clear();
			}

			// для фильтрации данных
			std::vector<std::string>	brokers;
			std::vector<std::string>	signals;
			std::string					currency;
			bool						use_demo	= true;		/// Использовать сделки на DEMO
			bool						use_real	= true;		/// Использовать сделки на REAL

			enum StatsTypes {
				FIRST_BET,
				LAST_BET,
				ALL_BET,
			};

			int							stats_type	= ALL_BET;	///	 Тип статистики (первая сделка, последняя, все сделки)

			// общая статистика
			WinrateStats total_stats;
			WinrateStats total_buy_stats;
			WinrateStats total_sell_stats;

			ChartData trades_profit;
			ChartData day_profit;
			ChartData trades_balance;
			ChartData day_balance;
			ChartData hour_balance;

			ChartData symbol_winrate;						/// Винрейт по символам
			ChartData symbol_delas;							/// Сделки по символам
			std::array<double,24> hour_winrate;
			std::array<double,24> hour_delas;

			double total_volume						= 0;	/// Общий объем сделок
			double total_profit						= 0;	/// Общий профит
			double total_gain						= 0;	/// Общий прирост

			double max_drawdown						= 0;	/// Максимальная относительная просадка
			double max_absolute_drawdown			= 0;	/// Максимальная абсолютная просадка
			uint64_t max_drawdown_date				= 0;	/// Дата максимальной просадки
			double aver_drawdown					= 0;	/// Средняя относительная просадка

			double aver_trade_size					= 0;
			double aver_absolute_trade_size			= 0;

			double aver_profit_per_trade			= 0;	/// Средний относительный профит на сделку
			double aver_absolute_profit_per_trade	= 0;	/// Средний абсолютный профит на сделку
			double max_absolute_profit_per_trade	= 0;	/// Максимальный абсолютный профит на сделку

			double gross_profit						= 0;
			double gross_loss						= 0;
			double profit_factor					= 0;

			// статистика профита
			std::array<double,24> profit_24h;				/// Заивимость профита от часа дня
			std::array<double,7>  profit_7wd;				/// Зависимость профита от дня недели
			std::array<double,31> profit_31d;				/// Зависимость профита от дня месяца
			std::array<double,12> profit_12m;				/// Зависимость профита от месяца года

			inline void win(
					const std::string &symbol,
					const ContractType contract_type,
					const ztime::timestamp_t timestamp) noexcept {
				total_stats.win();
				stats_symbol[symbol].win();
				if (contract_type == ContractType::BUY) {
					total_buy_stats.win();
				} else {
					total_sell_stats.win();
				}

				stats_year[ztime::get_first_timestamp_year(timestamp)].win();
				stats_month[ztime::get_month(timestamp)].win();
				stats_day_month[ztime::get_day_month(timestamp)].win();
				stats_hour_day[ztime::get_hour_day(timestamp)].win();
				stats_minute_day[ztime::get_minute_day(timestamp)].win();
				stats_week_day[ztime::get_weekday(timestamp)].win();

				if (counter_bet_timestamp == 0) counter_bet_timestamp = ztime::get_first_timestamp_minute(timestamp);

				if (counter_bet_timestamp == ztime::get_first_timestamp_minute(timestamp)) {
					stats_temp_counter_bet.win();
					++counter_bet;
				} else {
					counter_bet_timestamp = ztime::get_first_timestamp_minute(timestamp);
					stats_counter_bet[counter_bet] = stats_temp_counter_bet;
					stats_temp_counter_bet.clear();
					stats_temp_counter_bet.win();
					counter_bet = 1;
				}
			}

			inline void loss(
					const std::string &symbol,
					const ContractType contract_type,
					const ztime::timestamp_t timestamp) noexcept {

				total_stats.loss();
				stats_symbol[symbol].loss();
				if (contract_type == ContractType::BUY) {
					total_buy_stats.loss();
				} else {
					total_sell_stats.loss();
				}

				stats_year[ztime::get_first_timestamp_year(timestamp)].loss();
				stats_month[ztime::get_month(timestamp)].loss();
				stats_day_month[ztime::get_day_month(timestamp)].loss();
				stats_hour_day[ztime::get_hour_day(timestamp)].loss();
				stats_minute_day[ztime::get_minute_day(timestamp)].loss();
				stats_week_day[ztime::get_weekday(timestamp)].loss();

				if (counter_bet_timestamp == 0) counter_bet_timestamp = ztime::get_first_timestamp_minute(timestamp);

				if (counter_bet_timestamp == ztime::get_first_timestamp_minute(timestamp)) {
					stats_temp_counter_bet.loss();
					++counter_bet;
				} else {
					counter_bet_timestamp = ztime::get_first_timestamp_minute(timestamp);
					stats_counter_bet[counter_bet] = stats_temp_counter_bet;
					stats_temp_counter_bet.clear();
					stats_temp_counter_bet.loss();
					counter_bet = 1;
				}
			}

			inline void standoff(
					const std::string &symbol,
					const ContractType contract_type,
					const ztime::timestamp_t timestamp) noexcept {

				total_stats.standoff();
				stats_symbol[symbol].standoff();
				if (contract_type == ContractType::BUY) {
					total_buy_stats.standoff();
				} else {
					total_sell_stats.standoff();
				}

				stats_year[ztime::get_first_timestamp_year(timestamp)].standoff();
				stats_month[ztime::get_month(timestamp)].standoff();
				stats_day_month[ztime::get_day_month(timestamp)].standoff();
				stats_hour_day[ztime::get_hour_day(timestamp)].standoff();
				stats_minute_day[ztime::get_minute_day(timestamp)].standoff();
				stats_week_day[ztime::get_weekday(timestamp)].standoff();

				if (counter_bet_timestamp == 0) counter_bet_timestamp = ztime::get_first_timestamp_minute(timestamp);

				if (counter_bet_timestamp == ztime::get_first_timestamp_minute(timestamp)) {
					stats_temp_counter_bet.standoff();
					++counter_bet;
				} else {
					counter_bet_timestamp = ztime::get_first_timestamp_minute(timestamp);
					stats_counter_bet[counter_bet] = stats_temp_counter_bet;
					stats_temp_counter_bet.clear();
					stats_temp_counter_bet.standoff();
					counter_bet = 1;
				}
			}

			void clear() noexcept {
				temp_balance.clear();

				stats_symbol.clear();
				stats_temp_counter_bet.clear();
				counter_bet_timestamp	= 0;
				counter_bet				= 1;

				// статистика по периодам
				stats_year.clear();
				stats_month.clear();
				stats_day_month.clear();
				stats_hour_day.clear();
				stats_minute_day.clear();
				stats_week_day.clear();
				stats_expiration.clear();
				// одновременные сделки
				stats_counter_bet.clear();
				// статистика по всему
				total_stats.clear();
				total_buy_stats.clear();
				total_sell_stats.clear();
				// график
				trades_profit.clear();
				day_profit.clear();
				trades_balance.clear();
				day_balance.clear();
				hour_balance.clear();
				//
				symbol_winrate.clear();
				symbol_delas.clear();
				// остальное
				total_volume					= 0;
				total_profit					= 0;
				total_gain						= 0;

				max_drawdown					= 0;
				max_absolute_drawdown			= 0;
				max_drawdown_date				= 0;
				aver_drawdown					= 0;

				aver_profit_per_trade			= 0;
				aver_absolute_profit_per_trade	= 0;
				max_absolute_profit_per_trade	= 0;

				gross_profit					= 0;
				gross_loss						= 0;
				profit_factor					= 0;
				//
				std::fill(profit_24h.begin(),profit_24h.end(),0.0d);
				std::fill(profit_7wd.begin(),profit_7wd.end(),0.0d);
				std::fill(profit_31d.begin(),profit_31d.end(),0.0d);
				std::fill(profit_12m.begin(),profit_12m.end(),0.0d);
			}

			template<class T>
			void calc(const T &bets, const double start_balance) noexcept {
				clear();

				size_t counter_bet = 0;
				double profit = 0;

				for (auto &bet : bets) {

					if (stats_type == FIRST_BET && bet.step != 0) continue;
					if (stats_type == LAST_BET && !bet.last) continue;
					if (!currency.empty() && bet.currency != currency) continue;

					if (!brokers.empty()) {
						bool found = false;
						for (size_t i = 0; i < brokers.size(); ++i) {
							if (brokers[i] == bet.broker) {
								found = true;
								break;
							}
						}
						if (!found) continue;
					}

					if (!signals.empty()) {
						bool found = false;
						for (size_t i = 0; i < signals.size(); ++i) {
							if (signals[i] == bet.signal) {
								found = true;
								break;
							}
						}
						if (!found) continue;
					}

					if (bet.demo && !use_demo) continue;
					if (!bet.demo && !use_real) continue;
					if (bet.amount == 0) continue;

					const ztime::timestamp_t timestamp = bet.open_date / ztime::MILLISECONDS_IN_SECOND;
					const ztime::timestamp_t end_timestamp = bet.close_date / ztime::MILLISECONDS_IN_SECOND;
					const ztime::timestamp_t first_timestamp_day = ztime::get_first_timestamp_day(timestamp);
					const size_t hour = ztime::get_hour_day(timestamp);
					const size_t weekday = ztime::get_weekday(timestamp);
					const size_t day_month = ztime::get_day_month(timestamp);
					const size_t month = ztime::get_day_month(timestamp);

					if (bet.status == BetStatus::WIN) {
						win(bet.symbol, bet.contract_type, timestamp);
						profit += bet.profit;

						trades_profit.y_data.push_back(profit);
						trades_profit.x_data.push_back(timestamp);

						if (day_profit.x_data.empty() || day_profit.x_data.back() != first_timestamp_day) {
							day_profit.x_data.push_back(first_timestamp_day);
							day_profit.y_data.push_back(bet.profit);
						} else {
							day_profit.y_data.back() += bet.profit;
						}

						if (temp_balance.find(timestamp) == temp_balance.end()) {
							temp_balance[timestamp] = -bet.amount;
						} else {
							temp_balance[timestamp] += -bet.amount;
						}
						if (temp_balance.find(end_timestamp) == temp_balance.end()) {
							temp_balance[end_timestamp] = (bet.amount + bet.profit);
						} else {
							temp_balance[end_timestamp] += (bet.amount + bet.profit);
						}

						aver_profit_per_trade += bet.profit / bet.amount;
						aver_absolute_profit_per_trade += bet.profit;
						++counter_bet;
						if (bet.profit > max_absolute_profit_per_trade) max_absolute_profit_per_trade = bet.profit;

						gross_profit += bet.profit;
						total_profit += bet.profit;
						total_volume += bet.amount;

						profit_24h[hour] += bet.profit;
						profit_7wd[weekday] += bet.profit;
						profit_31d[day_month] += bet.profit;
						profit_12m[month] += bet.profit;

						aver_absolute_trade_size += bet.amount;
					} else
					if (bet.status == BetStatus::LOSS) {
						loss(bet.symbol, bet.contract_type, timestamp);
						profit -= bet.amount;

						trades_profit.y_data.push_back(profit);
						trades_profit.x_data.push_back(timestamp);

						if (day_profit.x_data.empty() || day_profit.x_data.back() != first_timestamp_day) {
							day_profit.x_data.push_back(first_timestamp_day);
							day_profit.y_data.push_back(-bet.amount);
						} else {
							day_profit.y_data.back() += -bet.amount;
						}

						if (temp_balance.find(timestamp) == temp_balance.end()) {
							temp_balance[timestamp] = -bet.amount;
						} else {
							temp_balance[timestamp] += -bet.amount;
						}
						if (temp_balance.find(end_timestamp) == temp_balance.end()) {
							temp_balance[end_timestamp] = 0;
						} else {
							temp_balance[end_timestamp] += 0;
						}

						aver_profit_per_trade -= 1.0;
						aver_absolute_profit_per_trade -= bet.amount;
						++counter_bet;

						gross_loss += bet.amount;
						total_profit -= bet.amount;
						total_volume += bet.amount;

						profit_24h[hour] -= bet.amount;
						profit_7wd[weekday] -= bet.amount;
						profit_31d[day_month] -= bet.amount;
						profit_12m[month] -= bet.amount;

						aver_absolute_trade_size += bet.amount;
					} else
					if (bet.status == BetStatus::STANDOFF) {
						standoff(bet.symbol, bet.contract_type, timestamp);

						profit += 0;
						trades_profit.y_data.push_back(profit);
						trades_profit.x_data.push_back(timestamp);

						if (day_profit.x_data.empty() || day_profit.x_data.back() != first_timestamp_day) {
							day_profit.x_data.push_back(first_timestamp_day);
							day_profit.y_data.push_back(0);
						}

						if (temp_balance.find(timestamp) == temp_balance.end()) {
							temp_balance[timestamp] = -bet.amount;
						} else {
							temp_balance[timestamp] += -bet.amount;
						}
						if (temp_balance.find(end_timestamp) == temp_balance.end()) {
							temp_balance[end_timestamp] = bet.amount;
						} else {
							temp_balance[end_timestamp] += bet.amount;
						}
						//aver_profit_per_trade += 0.0;
						//aver_absolute_profit_per_trade += 0;
						++counter_bet;

						//gross_loss += 0;
						//total_profit += 0;
						total_volume += bet.amount;

						//profit_24h[hour] += 0;
						//profit_7wd[weekday] += 0;
						//profit_31d[day_month] += 0;
						//profit_12m[month] += 0;

						aver_absolute_trade_size += bet.amount;
					}
				}

				if (counter_bet) {
					aver_absolute_profit_per_trade /= (double)counter_bet;
					aver_profit_per_trade /= (double)counter_bet;
					aver_absolute_trade_size /= (double)counter_bet;
				}

				if (gross_loss != 0) {
					profit_factor = gross_profit / gross_loss;
				} else {
					profit_factor = gross_profit != 0 ? std::numeric_limits<double>::max() : 0;
				}

				// рисуем график баланса
				if (!temp_balance.empty() && start_balance != 0) {

					double balance = start_balance;
					double last_max_balance = start_balance;
					double diff_balance = 0;

					max_absolute_drawdown = 0;
					max_drawdown = 0;
					aver_drawdown = 0;
					size_t counter_aver_drawdown = 0;

					bool is_drawdown = false;

					trades_balance.x_data.push_back(ztime::get_first_timestamp_day(temp_balance.begin()->first));
					trades_balance.y_data.push_back(balance);
					for (auto &b : temp_balance) {
						const double prev_balance = balance;
						balance += b.second;
						trades_balance.x_data.push_back(b.first);
						trades_balance.y_data.push_back(balance);

						if (balance < last_max_balance) {
							// замер разницы депозита между мин. и макс.
							diff_balance = last_max_balance - balance;
							// замер абсолютной и относительной просадки
							if (diff_balance > max_absolute_drawdown) {
								max_absolute_drawdown = diff_balance;
								max_drawdown = max_absolute_drawdown / last_max_balance;
								// запоминаемдату начала просадки
								if (!is_drawdown) max_drawdown_date = b.first;
							}
							// ставим флаг просадки
							is_drawdown = true;
						}
						if (balance >= last_max_balance) {
							// замер средней просадки
							if (is_drawdown) {
								aver_drawdown += (diff_balance / last_max_balance);
								++counter_aver_drawdown;
							}
							// сброс флагов и параметров
							last_max_balance = balance;
							is_drawdown = false;
						}
					}
					if (counter_aver_drawdown) aver_drawdown /= (double)counter_aver_drawdown;
					total_gain = balance / start_balance;
				} // if (!temp_balance.empty() && start_balance != 0)

				total_stats.calc();
				total_buy_stats.calc();
				total_sell_stats.calc();

				//> винрейт по символам
				for (auto &item : stats_symbol) {
					item.second.calc();
					symbol_winrate.x_label.push_back(item.first);
					symbol_winrate.y_data.push_back(item.second.winrate * 100);
					symbol_delas.x_label.push_back(item.first);
					symbol_delas.y_data.push_back(item.second.deals);
				} //<

			}
		}; // BetStats
#endif
		using BetStats = bet_database::BetStats;
		using BetMetaStats = bet_database::BetMetaStats;

		#if (0)
		/** \brief Класс для получения сведений о данных массива сделок
		 */
		class MetaBetStats {
		public:
			std::vector<std::string>	brokers;
			std::vector<std::string>	symbols;
			std::vector<std::string>	signals;
			std::vector<std::string>	currencies;
			bool						real		= false;
			bool						demo		= false;

			std::vector<BetStats>		currency_stats;
			std::vector<BetStats>		signals_stats;
			std::vector<BetStats>		brokers_stats;

			template<class T>
			void calc(const T &bets) noexcept {
				std::set<std::string> calc_currencies;
				std::set<std::string> calc_brokers;
				std::set<std::string> calc_signals;
				std::set<std::string> calc_symbols;

				for (auto &bet : bets) {
					calc_currencies.insert(bet.currency);
					calc_brokers.insert(bet.broker);
					calc_signals.insert(bet.signal);
					calc_symbols.insert(bet.symbol);
					if (bet.demo) demo = true;
					else real = true;
				}

				brokers = std::vector<std::string>(calc_brokers.begin(), calc_brokers.end());
				currencies = std::vector<std::string>(calc_currencies.begin(), calc_currencies.end());
				signals = std::vector<std::string>(calc_signals.begin(), calc_signals.end());
				symbols = std::vector<std::string>(calc_symbols.begin(), calc_symbols.end());

				std::cout << "-brokers" << brokers.size() << std::endl;
				std::cout << "-currencies" << currencies.size() << std::endl;
				std::cout << "-signals" << signals.size() << std::endl;
				std::cout << "-symbols" << symbols.size() << std::endl;

				currency_stats.clear();
				signals_stats.clear();
				brokers_stats.clear();

				std::cout << "-currency_stats.resize" << std::endl;
				currency_stats.resize(currencies.size());
				std::cout << "-currency_stats.size " << currency_stats.size() << std::endl;
				std::cout << "-signals_stats.resize" << std::endl;
				signals_stats.resize(currencies.size() * signals.size());
				std::cout << "-brokers_stats.resize" << std::endl;
				brokers_stats.resize(currencies.size() * brokers.size());
				std::cout << "-for" << std::endl;


				for (size_t c = 0; c < currencies.size(); ++c) {
					std::cout << "-2: " << c << " " << currencies[c] << std::endl;
					currency_stats[c].currency = currencies[c];

					std::cout << "-3: " << c << std::endl;
					//std::cout << "-3: " << currency_stats[c]. << std::endl;
					currency_stats[c].calc(bets, 0);
					std::cout << "-4: " << c << std::endl;
					/*
					std::cout << "-4: " << c << std::endl;
					for (size_t s = 0; s < signals.size(); ++s) {
						signals_stats[c+s*currencies.size()].currency = currencies[c];
						signals_stats[c+s*currencies.size()].signals.push_back(signals[s]);
						signals_stats[c+s*currencies.size()].calc(bets, 0);
					}
					for (size_t b = 0; b < brokers.size(); ++b) {
						brokers_stats[c+b*currencies.size()].currency = currencies[c];
						brokers_stats[c+b*currencies.size()].brokers.push_back(brokers[b]);
						brokers_stats[c+b*currencies.size()].calc(bets, 0);
					}
					*/
				}
			}
		}; // MetaBetStats
		#endif
	};

}; // trading_db

#endif // TRADING_DB_KEY_VALUE_DATABASE_HPP_INCLUDED
