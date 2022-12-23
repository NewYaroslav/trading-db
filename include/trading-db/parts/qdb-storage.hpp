#pragma once
#ifndef TRADING_DB_QDB_STORAGE_HPP_INCLUDED
#define TRADING_DB_QDB_STORAGE_HPP_INCLUDED

#if SQLITE_THREADSAFE != 1
#error "The project must be built for sqlite multithreading! Set the SQLITE_THREADSAFE=1"
#endif

#include "../config.hpp"

#include "../utility/sqlite-func.hpp"
#include "../utility/async-tasks.hpp"
#include "../utility/safe-queue.hpp"
#include "../utility/print.hpp"
#include "../utility/files.hpp"

#include <mutex>
#include <atomic>
#include <string>
#include <vector>
#include <map>

namespace trading_db {

	class QdbStorage {
	public:

		/** \brief Структура Ключ-Значение
		 */
		class KeyValue {
		public:
			uint64_t				key;
			std::vector<uint8_t>	value;
		};

		/** \brief Класс для хранения мета данных
		 */
		class MetaData {
		public:
			std::string key;
			std::string value;

			MetaData() {};

			MetaData(const std::string &k, const std::string &v) : key(k), value(v) {};
		};

		enum class METADATA_TYPE {
			SYMBOL_NAME,
			SYMBOL_DIGITS,
			SYMBOL_DATA_FEED_SOURCE,
		};

		/** \brief Класс конфигурации базы данных
		 */
		class Config {
		public:
			const std::string title = "trading_db::QdbStorage ";	/**< Название заголовка для логов */
			const std::string candle_table		= "candles";		/**< Имя таблицы */
			const std::string tick_table		= "ticks";			/**< Имя таблицы */
			const std::string meta_data_table	= "meta-data";		/**< Имя таблицы */
			int busy_timeout = 0;
			std::atomic<bool> use_log = ATOMIC_VAR_INIT(false);
		};

		Config config;	/**< Конфигурация базы данных */

	private:

		std::string database_name;
		sqlite3 *sqlite_db = nullptr;
		// команды для транзакций
		utility::SqliteTransaction sqlite_transaction;
		// предкомпилированные команды
		utility::SqliteStmt stmt_replace_candle;
		utility::SqliteStmt stmt_replace_tick;
		utility::SqliteStmt stmt_replace_meta_data;
		//
		utility::SqliteStmt stmt_get_candle;
		utility::SqliteStmt stmt_get_tick;
		utility::SqliteStmt stmt_get_meta_data;

		// флаг сброса
		bool is_backup = ATOMIC_VAR_INIT(false);
		std::mutex backup_mutex;
		// флаг сброса
		std::atomic<bool> is_shutdown = ATOMIC_VAR_INIT(false);
		//
		std::mutex method_mutex;

		inline void print_error(
				const std::string message,
				const int line) noexcept {
			if (config.use_log) {
				TRADING_DB_TICK_DB_PRINT
					<< config.title << "error in [file " << __FILE__
					<< ", line " << line
					<< "], message: " << message << std::endl;
			}
		}

		bool open_db(
				sqlite3 *&sqlite_db_ptr,
				const std::string &db_name,
				const bool readonly = false) noexcept {
			utility::create_directory(db_name, true);
			// открываем и возможно еще создаем таблицу
			int flags = readonly ?
				(SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX) :
				(SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX);
			if(sqlite3_open_v2(db_name.c_str(), &sqlite_db_ptr, flags, nullptr) != SQLITE_OK) {
				print_error(std::string(sqlite3_errmsg(sqlite_db_ptr)) +
					", db name " + db_name, __LINE__);
				return false;
			} else {
				sqlite3_busy_timeout(sqlite_db_ptr, config.busy_timeout);
				// создаем таблицы в базе данных, если они еще не созданы
				const std::string create_candle_table_sql =
					"CREATE TABLE IF NOT EXISTS '" + config.candle_table + "' ("
					"key				INTEGER PRIMARY KEY NOT NULL,"
					"value				BLOB				NOT NULL)";
				const std::string create_tick_table_sql =
					"CREATE TABLE IF NOT EXISTS '" + config.tick_table + "' ("
					"key				INTEGER PRIMARY KEY NOT NULL,"
					"value				BLOB				NOT NULL)";
				const std::string create_meta_data_table_sql =
					"CREATE TABLE IF NOT EXISTS '" + config.meta_data_table + "' ("
					"key				TEXT	PRIMARY KEY NOT NULL,"
					"value				TEXT				NOT NULL)";

				if (!utility::prepare(sqlite_db_ptr, create_candle_table_sql)) return false;
				if (!utility::prepare(sqlite_db_ptr, create_tick_table_sql)) return false;
				if (!utility::prepare(sqlite_db_ptr, create_meta_data_table_sql)) return false;
			}
			return true;
		}

		bool init_db(
				const std::string &db_name,
				const bool readonly = false) noexcept {
			if (!open_db(sqlite_db, db_name, readonly)) {
				sqlite3_close_v2(sqlite_db);
				sqlite_db = nullptr;
				return false;
			}
			if (!sqlite_transaction.init(sqlite_db) ||
				!stmt_replace_candle.init(sqlite_db, "INSERT OR REPLACE INTO '" + config.candle_table + "' (key, value) VALUES (?, ?)") ||
				!stmt_replace_tick.init(sqlite_db, "INSERT OR REPLACE INTO '" + config.tick_table + "' (key, value) VALUES (?, ?)") ||
				!stmt_replace_meta_data.init(sqlite_db, "INSERT OR REPLACE INTO '" + config.meta_data_table + "' (key, value) VALUES (?, ?)") ||
				!stmt_get_candle.init(sqlite_db, "SELECT value FROM '" + config.candle_table + "' WHERE key == :x") ||
				!stmt_get_tick.init(sqlite_db, "SELECT value FROM '" + config.tick_table + "' WHERE key == :x") ||
				!stmt_get_meta_data.init(sqlite_db, "SELECT value FROM '" + config.meta_data_table + "' WHERE key == :x")
				) {
				sqlite3_close_v2(sqlite_db);
				sqlite_db = nullptr;
				print_error("stmt init return false", __LINE__);
				return false;
			}
			return true;
		}

		bool replace_price_data(
				const uint64_t key,
				const std::vector<uint8_t> &buffer,
				utility::SqliteTransaction &transaction,
				utility::SqliteStmt &stmt) noexcept {

			if (!transaction.begin_transaction()) return false;
			sqlite3_reset(stmt.get());

			if (sqlite3_bind_int64(stmt.get(), 1, key) != SQLITE_OK) {
				transaction.rollback();
				return false;
			}

			if (sqlite3_bind_blob(stmt.get(), 2, buffer.data(), buffer.size(), SQLITE_STATIC) != SQLITE_OK) {
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
				print_error(std::string(sqlite3_errmsg(sqlite_db)) +
					", code " + std::to_string(err), __LINE__);
				return false;
			}
			if (!transaction.commit()) return false;
			return true;
		}

		bool replace_meta_data(
				const MetaData &pair,
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

		bool replace_price_data_map(
				const std::map<uint64_t, std::vector<uint8_t>>	&buffer,
				utility::SqliteTransaction						&transaction,
				utility::SqliteStmt								&stmt) noexcept {
			if (buffer.empty()) return true;
			if (!transaction.begin_transaction()) return false;
			sqlite3_reset(stmt.get());
			for (const auto &pair : buffer) {
				if (sqlite3_bind_int64(stmt.get(), 1, pair.first) != SQLITE_OK) {
					transaction.rollback();
					return false;
				}
				if (sqlite3_bind_blob(stmt.get(), 2, pair.second.data(), pair.second.size(), SQLITE_STATIC) != SQLITE_OK) {
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
					transaction.rollback();
					print_error("sqlite3_step return SQLITE_BUSY", __LINE__);
					return false;
				} else {
					transaction.rollback();
					print_error(std::string(sqlite3_errmsg(sqlite_db)) +
						", code " + std::to_string(err), __LINE__);
					return false;
				}
			}
			if (!transaction.commit()) return false;
			return true;
		}

		inline std::vector<uint8_t> get_price_data(utility::SqliteStmt &stmt, const uint64_t key) noexcept {
			std::vector<uint8_t> value;
			int err = 0;
			while (true) {
				if ((err = sqlite3_reset(stmt.get())) != SQLITE_OK) {
					print_error("sqlite3_reset return code " + std::to_string(err), __LINE__);
					return std::vector<uint8_t>();
				}
				if (sqlite3_bind_int64(stmt.get(), 1, key) != SQLITE_OK) {
					print_error("sqlite3_bind_text error", __LINE__);
					return std::vector<uint8_t>();
				}
				err = sqlite3_step(stmt.get());
				if(err == SQLITE_DONE) {
					sqlite3_reset(stmt.get());
					sqlite3_clear_bindings(stmt.get());
					return std::vector<uint8_t>();
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
					return std::vector<uint8_t>();
				}

				const void* blob = sqlite3_column_blob(stmt.get(), 0);
				const size_t blob_bytes = sqlite3_column_bytes(stmt.get(), 0);
				value.assign((const uint8_t*)blob, (const uint8_t*)blob + blob_bytes);

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
					value = std::vector<uint8_t>();
					print_error("sqlite3_step return SQLITE_BUSY", __LINE__);
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
					continue;
				}
				break;
			}
			return std::move(value);
		}

		inline MetaData get_meta_data(
				utility::SqliteStmt &stmt,
				const std::string &key) noexcept {

			MetaData meta_data;
			meta_data.key = key;
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

				meta_data.value = (const char *)sqlite3_column_text(stmt.get(),0);
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

		inline bool get_uint64_value(utility::SqliteStmt &stmt, uint64_t &value) noexcept {
			int err = 0;
			while (true) {
				if ((err = sqlite3_reset(stmt.get())) != SQLITE_OK) {
					print_error("sqlite3_reset return code " + std::to_string(err), __LINE__);
					return false;
				}

				err = sqlite3_step(stmt.get());
				if(err == SQLITE_DONE) {
					sqlite3_reset(stmt.get());
					return false;
				} else
				if(err == SQLITE_BUSY) {
					sqlite3_reset(stmt.get());
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
					continue;
				} else
				if (err != SQLITE_ROW) {
					sqlite3_reset(stmt.get());
					print_error("sqlite3_step return code " + std::to_string(err), __LINE__);
					return false;
				}

				value = (uint64_t)sqlite3_column_int64(stmt.get(), 0);
				err = sqlite3_step(stmt.get());

				if (err == SQLITE_ROW) {
					break;
				} else
				if(err == SQLITE_DONE) {
					sqlite3_reset(stmt.get());
					break;
				} else
				if(err == SQLITE_BUSY) {
					sqlite3_reset(stmt.get());
					print_error("sqlite3_step return SQLITE_BUSY", __LINE__);
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
					continue;
				}
				break;
			}

			return true;
		}

		const bool check_init_db() noexcept {
			return (sqlite_db != nullptr);
		}

	public:

		QdbStorage() {};

		~QdbStorage() {
			is_shutdown = true;
			std::lock_guard<std::mutex> lock(method_mutex);
			if (sqlite_db != nullptr) sqlite3_close_v2(sqlite_db);
		};

		/** \brief Open database
		 * \param path		Path to the database file
		 * \param readonly	'read-only' flag
		 * \return Will return true if initialization is successful
		 */
		inline bool open(const std::string &path, const bool readonly = false) noexcept {
			std::lock_guard<std::mutex> lock(method_mutex);
			if (check_init_db()) return false;
			return init_db(path, readonly);
		}

		inline bool get_min_max_date(const bool use_tick_data, uint64_t &t_min, uint64_t &t_max) {
            utility::SqliteStmt stmt_min;
            utility::SqliteStmt stmt_max;
            if (use_tick_data) {
                stmt_min.init(sqlite_db, "SELECT MIN(key) as min FROM '" + config.tick_table + "'");
                stmt_max.init(sqlite_db, "SELECT MAX(key) as max FROM '" + config.tick_table + "'");
                if (get_uint64_value(stmt_min, t_min) && get_uint64_value(stmt_max, t_max)) {
                    t_max += ztime::SECONDS_IN_HOUR;
                    return true;
                }
            } else {
                stmt_min.init(sqlite_db, "SELECT MIN(key) as min FROM '" + config.candle_table + "'");
                stmt_max.init(sqlite_db, "SELECT MAX(key) as max FROM '" + config.candle_table + "'");
                if (get_uint64_value(stmt_min, t_min) && get_uint64_value(stmt_max, t_max)) {
                    t_max += ztime::SECONDS_IN_DAY;
                    return true;
                }
            }
            t_min = t_max = 0;
            return false;
		}

		inline bool read_candles(std::vector<uint8_t> &data, const uint64_t t) noexcept {
			data = get_price_data(stmt_get_candle, t);
			if (data.empty()) return false;
			return true;
		}

		inline bool read_ticks(std::vector<uint8_t> &data, const uint64_t t) noexcept {
			data = get_price_data(stmt_get_tick, t);
			if (data.empty()) return false;
			return true;
		}

		inline bool write_candles(const std::map<uint64_t, std::vector<uint8_t>> &data) noexcept {
			{
				std::lock_guard<std::mutex> lock(method_mutex);
				if (!check_init_db()) return false;
			}
			while (!is_shutdown) {
				{
					std::lock_guard<std::mutex> lock(method_mutex);
					if (replace_price_data_map(data, sqlite_transaction, stmt_replace_candle)) return true;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			return false;
		}

		inline bool write_ticks(const std::map<uint64_t, std::vector<uint8_t>> &data) noexcept {
			{
				std::lock_guard<std::mutex> lock(method_mutex);
				if (!check_init_db()) return false;
			}
			while (!is_shutdown) {
				{
					std::lock_guard<std::mutex> lock(method_mutex);
					if (replace_price_data_map(data, sqlite_transaction, stmt_replace_tick)) return true;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			return false;
		}

		inline bool write_candles(const std::vector<uint8_t> &data, const uint64_t t) noexcept {
			{
				std::lock_guard<std::mutex> lock(method_mutex);
				if (!check_init_db()) return false;
			}
			while (!is_shutdown) {
				{
					std::lock_guard<std::mutex> lock(method_mutex);
					if (replace_price_data(t, data, sqlite_transaction, stmt_replace_candle)) return true;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			return false;
		}

		inline bool write_ticks(const std::vector<uint8_t> &data, const uint64_t t) noexcept {
			{
				std::lock_guard<std::mutex> lock(method_mutex);
				if (!check_init_db()) return false;
			}
			while (!is_shutdown) {
				{
					std::lock_guard<std::mutex> lock(method_mutex);
					if (replace_price_data(t, data, sqlite_transaction, stmt_replace_tick)) return true;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			return false;
		}

		inline bool remove_candles(const uint64_t t) noexcept {
			std::lock_guard<std::mutex> lock(method_mutex);
			if (!check_init_db()) return false;
			return utility::prepare(sqlite_db, "DELETE FROM '" + config.candle_table + "' WHERE key == " + std::to_string(t));
		}

		inline bool remove_ticks(const uint64_t t) noexcept {
			std::lock_guard<std::mutex> lock(method_mutex);
			if (!check_init_db()) return false;
			return utility::prepare(sqlite_db, "DELETE FROM '" + config.tick_table + "' WHERE key == " + std::to_string(t));
		}

		/** \brief Удалить все данные
		 */
		inline bool remove_all() noexcept {
			std::lock_guard<std::mutex> lock(method_mutex);
			if (!check_init_db()) return false;
			return
				utility::prepare(sqlite_db, "DELETE FROM '" + config.candle_table + "'") &&
				utility::prepare(sqlite_db, "DELETE FROM '" + config.tick_table + "'") &&
				utility::prepare(sqlite_db, "DELETE FROM '" + config.meta_data_table + "'");
		}

		inline std::string get_info_str(const METADATA_TYPE type) noexcept {
			MetaData pair;
			switch (type) {
			case METADATA_TYPE::SYMBOL_NAME:
				pair = get_meta_data(stmt_get_meta_data, "SYMBOL_NAME");
				break;
			case METADATA_TYPE::SYMBOL_DATA_FEED_SOURCE:
				pair = get_meta_data(stmt_get_meta_data, "SYMBOL_DATA_FEED_SOURCE");
				break;
			case METADATA_TYPE::SYMBOL_DIGITS:
			default:
				break;
			};
			return pair.value;
		}

		inline int get_info_int(const METADATA_TYPE type) noexcept {
			MetaData pair;
			switch (type) {
			case METADATA_TYPE::SYMBOL_DIGITS:
				pair = get_meta_data(stmt_get_meta_data, "SYMBOL_DIGITS");
				break;
			default:
				return 0;
			};
			if (pair.value.empty()) return 0;
			return std::stoi(pair.value);
		}

		inline bool set_info_str(const METADATA_TYPE type, const std::string &value) {
			MetaData pair;
			pair.value = value;
			switch (type) {
			case METADATA_TYPE::SYMBOL_NAME:
				pair.key = "SYMBOL_NAME";
				break;
			case METADATA_TYPE::SYMBOL_DATA_FEED_SOURCE:
				pair.key = "SYMBOL_DATA_FEED_SOURCE";
				break;
			default:
				return false;
			};
			return replace_meta_data(pair, sqlite_transaction, stmt_replace_meta_data);
		}

		inline bool set_info_int(const METADATA_TYPE type, const int value) {
			MetaData pair;
			pair.value = std::to_string(value);
			switch (type) {
			case METADATA_TYPE::SYMBOL_DIGITS:
				pair.key = "SYMBOL_DIGITS";
				break;
			default:
				return false;
			};
			return replace_meta_data(pair, sqlite_transaction, stmt_replace_meta_data);
		}

	}; // QdbStorage
}; // trading_db

#endif // TRADING_DB_QDB_STORAGE_HPP_INCLUDED
