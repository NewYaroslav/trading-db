#pragma once
#ifndef TRADING_DB_KEY_VALUE_DATABASE_HPP_INCLUDED
#define TRADING_DB_KEY_VALUE_DATABASE_HPP_INCLUDED

#if SQLITE_THREADSAFE != 1
#error "The project must be built for sqlite multithreading! Set the SQLITE_THREADSAFE=1"
#endif

#include "config.hpp"
#include "utils/sqlite-func.hpp"
#include "utils/async-tasks.hpp"
#include "utils/print.hpp"
#include "utils/files.hpp"
#include <mutex>
#include <atomic>
#include <future>
#include <vector>
#include <map>

namespace trading_db {

	/** \brief Класс базы данных типа Ineger Key - Blob Value
	 * Данный класс нужен как вспомогательный для других проектов
	 */
	template<class BTYPE>
	class IntKeyBlobValueDatabase {
	public:

		/** \brief Структура Ключ-Значение
		 */
		class KeyValue {
		public:
			int64_t key;
			BTYPE value;
		};

		/** \brief Класс конфигурации базы данных
		 */
		class Config {
		public:
			std::string title = "trading_db::IntKeyBlobValueDatabase "; /**< Название заголовка для логов */
			std::string table = "Data";									/**< Имя таблицы */
			int busy_timeout = 0;
			std::atomic<bool> use_log = ATOMIC_VAR_INIT(false);
		};

		Config config;	/**< Конфигурация базы данных */

	private:

		std::string database_name;
		sqlite3 *sqlite_db = nullptr;
		// команды для транзакций
		utils::SqliteTransaction sqlite_transaction;
		// предкомпилированные команды
		utils::SqliteStmt stmt_replace_key_value;
		utils::SqliteStmt stmt_get_key_value;
		utils::SqliteStmt stmt_get_all_key_value;
		// флаг сброса
		bool is_backup = ATOMIC_VAR_INIT(false);
		std::mutex backup_mutex;
		// флаг сброса
		std::atomic<bool> is_shutdown = ATOMIC_VAR_INIT(false);

		std::mutex method_mutex;

		utils::AsyncTasks async_tasks;

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
			utils::create_directory(db_name, true);
			// открываем и возможно еще создаем таблицу
			int flags = readonly ?
				(SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX) :
				(SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX);
			if(sqlite3_open_v2(db_name.c_str(), &sqlite_db_ptr, flags, nullptr) != SQLITE_OK) {
				sqlite3_close_v2(sqlite_db_ptr);
				sqlite_db_ptr = nullptr;
				print_error(std::string(sqlite3_errmsg(sqlite_db_ptr)) +
					", db name " + db_name, __LINE__);
				return false;
			} else {
				sqlite3_busy_timeout(sqlite_db_ptr, config.busy_timeout);
				// создаем таблицу в базе данных, если она еще не создана
				const std::string create_key_value_table_sql =
					"CREATE TABLE IF NOT EXISTS '" + config.table + "' ("
					"key				INTEGER PRIMARY KEY NOT NULL,"
					"value				BLOB				NOT NULL)";

				if (!utils::prepare(sqlite_db_ptr, create_key_value_table_sql)) {
					print_error("utils::prepare return false", __LINE__);
					return false;
				}
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
				!stmt_replace_key_value.init(sqlite_db, "INSERT OR REPLACE INTO '" + config.table + "' (key, value) VALUES (?, ?)") ||
				!stmt_get_key_value.init(sqlite_db, "SELECT * FROM '" + config.table + "' WHERE key == :x") ||
				!stmt_get_all_key_value.init(sqlite_db, "SELECT value FROM '" + config.table + "'")) {
				sqlite3_close_v2(sqlite_db);
				sqlite_db = nullptr;
				print_error("stmt init return false", __LINE__);
				return false;
			}
			return true;
		}

		template<class T>
		bool replace(T &pair, utils::SqliteTransaction &transaction, utils::SqliteStmt &stmt) noexcept {
			if (!transaction.begin_transaction()) return false;
			sqlite3_reset(stmt.get());
			if (sqlite3_bind_int64(stmt.get(), 1, pair.key) != SQLITE_OK) {
				transaction.rollback();
				return false;
			}
			if (sqlite3_bind_blob(stmt.get(), 2, pair.value.data(), pair.value.size(), SQLITE_STATIC) != SQLITE_OK) {
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

		bool replace_map(
				const std::map<int64_t, BTYPE> &buffer,
				utils::SqliteTransaction &transaction,
				utils::SqliteStmt &stmt) noexcept {
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

		template<class T>
		inline T get_value_from_db(utils::SqliteStmt &stmt, const int64_t key) noexcept {
			T value;
			int err = 0;
			while (true) {
				if ((err = sqlite3_reset(stmt.get())) != SQLITE_OK) {
					print_error("sqlite3_reset return code " + std::to_string(err), __LINE__);
					return T();
				}
				if (sqlite3_bind_int64(stmt.get(), 1, key) != SQLITE_OK) {
					print_error("sqlite3_bind_text error", __LINE__);
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

				const void* blob = sqlite3_column_blob(stmt.get(), 0);
				const size_t blob_bytes = sqlite3_column_bytes(stmt.get(), 0);
				value.assign(blob, blob + blob_bytes);

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
					value = T();
					print_error("sqlite3_step return SQLITE_BUSY", __LINE__);
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
					continue;
				}
				break;
			}
			return std::move(value);
		}

		template<class T>
		inline T get_values_from_db(utils::SqliteStmt &stmt) {
			T buffer;
			int err = 0;
			while (true) {
				if ((err = sqlite3_reset(stmt.get())) != SQLITE_OK) {
					print_error("sqlite3_reset return code " +
						std::to_string(err), __LINE__);
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
					return T();
				}

				err = 0;
				while (true) {
					const uint8_t *blob = (const uint8_t*)sqlite3_column_blob(stmt.get(), 0);
					const size_t blob_bytes = sqlite3_column_bytes(stmt.get(), 0);
					buffer.emplace_back(blob, blob + blob_bytes);
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

		inline std::map<int64_t, BTYPE> get_map_pairs_from_db(utils::SqliteStmt &stmt) {
			std::map<int64_t, BTYPE> buffer;
			int err = 0;
			while (true) {
				if ((err = sqlite3_reset(stmt.get())) != SQLITE_OK) {
					print_error("sqlite3_reset return code " + std::to_string(err), __LINE__);
					return std::move(buffer);
				}
				err = sqlite3_step(stmt.get());
				if(err == SQLITE_DONE) {
					sqlite3_reset(stmt.get());
					sqlite3_clear_bindings(stmt.get());
					return std::move(buffer);
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
					return std::move(buffer);
				}

				err = 0;
				while (true) {
					const int64_t key = sqlite3_column_int64(stmt.get(), 0);

					const uint8_t* blob = (const uint8_t*)sqlite3_column_blob(stmt.get(), 1);
					const size_t blob_bytes = sqlite3_column_bytes(stmt.get(), 1);

					buffer[key].assign(blob, blob + blob_bytes);

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
						print_error("sqlite3_step return SQLITE_BUSY", __LINE__);
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

		const bool check_init_db() noexcept {
			return (sqlite_db != nullptr);
		}

	public:

		IntKeyBlobValueDatabase() {};

		~IntKeyBlobValueDatabase() {
			is_shutdown = true;
			async_tasks.clear();
			std::lock_guard<std::mutex> lock(method_mutex);
			if (sqlite_db != nullptr) sqlite3_close_v2(sqlite_db);
		}

		/** \brief Инициализировать хранилище пар "ключ-значение"
		 * \param path	Путь к файлу
		 * \param readonly	Флаг "только чтение"
		 * \param use_log	Включить вывод логов
		 */
		bool init(
				const std::string &path,
				const bool readonly = false) noexcept {
			std::lock_guard<std::mutex> lock(method_mutex);
			if (check_init_db()) return false;
			database_name = path;
			return init_db(path, readonly);
		}

		/** \brief Создать бэкап
		 * \param path	Путь к бэкапу базы данных
		 * \return Вернет true, если бэкап начался
		 */
		bool backup(
				const std::string &path,
				const std::function<void(const std::string &path, const bool is_error)> callback) noexcept {
			{
				std::lock_guard<std::mutex> lock(method_mutex);
				if (!check_init_db()) return false;
			}
			{
				std::lock_guard<std::mutex> lock(backup_mutex);
				if (is_backup) return false;
				is_backup = true;
			}
			async_tasks.create_task([&, path]() {
				if (!utils::backup_form_db(path, this->sqlite_db)) {
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

		/** \brief Установить пару "ключ-значение"
		 * \param key	Ключ пары "ключ-значение"
		 * \param value Значение пары "ключ-значение"
		 * \return Вернет true в случае успешного завершения
		 */
		inline bool set_pair(const int64_t key, const BTYPE &value) noexcept {
			{
				std::lock_guard<std::mutex> lock(method_mutex);
				if (!check_init_db()) return false;
			}
			const KeyValue &pair{key, value};
			while (!is_shutdown) {
				{
					std::lock_guard<std::mutex> lock(method_mutex);
					if (replace(pair, sqlite_transaction, stmt_replace_key_value)) return true;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			return false;
		}

		/** \brief Установить пару "ключ-значение"
		 * \param pair	Объект "ключ-значение"
		 * \return Вернет true в случае успешного завершения
		 */
		template<class T>
		inline bool set_pair(const T& pair) noexcept {
			{
				std::lock_guard<std::mutex> lock(method_mutex);
				if (!check_init_db()) return false;
			}
			while (!is_shutdown) {
				{
					std::lock_guard<std::mutex> lock(method_mutex);
					if (replace(pair, sqlite_transaction, stmt_replace_key_value)) return true;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			return false;
		}

		/** \brief Установить пары "ключ-значение" из map
		 * \param pairs Массив пар "ключ-значение"
		 * \return Вернет true в случае успешного завершения
		 */
		inline bool set_map_pairs(const std::map<int64_t, BTYPE> &pairs) noexcept {
			{
				std::lock_guard<std::mutex> lock(method_mutex);
				if (!check_init_db()) return false;
			}
			while (!is_shutdown) {
				{
					std::lock_guard<std::mutex> lock(method_mutex);
					if (replace_map(pairs, sqlite_transaction, stmt_replace_key_value)) return true;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			return false;
		}

		/** \brief Получить значение по ключу
		 * \param key	Ключ заметки
		 * \return Значение по ключу
		 */
		inline BTYPE get_value(const int64_t key) noexcept {
			std::lock_guard<std::mutex> lock(method_mutex);
			if (!check_init_db()) return BTYPE();
			return get_value_from_db(stmt_get_key_value, key);
		}

		/** \brief Получить весь список пар "ключ-значение"
		 * \return Массив пар "ключ-значение"
		 */
		template<class T>
		inline T get_all_values() noexcept {
			std::lock_guard<std::mutex> lock(method_mutex);
			if (!check_init_db()) return T();
			return get_values_from_db<T>(stmt_get_all_key_value);
		}

		/** \brief Получить весь список пар "ключ-значение" в виде map
		 * \return Массив пар "ключ-значение"
		 */
		std::map<int64_t, BTYPE> get_map_all_pairs() noexcept {
			std::lock_guard<std::mutex> lock(method_mutex);
			if (!check_init_db()) return std::map<int64_t, BTYPE>();
			return get_map_pairs_from_db(stmt_get_all_key_value);
		}

		/** \brief Удалить все данные
		 */
		inline bool remove_all() noexcept {
			std::lock_guard<std::mutex> lock(method_mutex);
			if (!check_init_db()) return false;
			return utils::prepare(sqlite_db, "DELETE FROM '" + config.table + "'");
		}

		/** \brief Удалить значение по ключу
		 * \param key	Ключ пары "ключ-значение"
		 */
		inline bool remove_value(const int64_t key) noexcept {
			std::lock_guard<std::mutex> lock(method_mutex);
			if (!check_init_db()) return false;
			return utils::prepare(sqlite_db, "DELETE FROM '" + config.table + "' WHERE key == " + std::to_string(key));
		}

		/** \brief Удалить пару "ключ-значение"
		 * \param pair	Пары "ключ-значение"
		 */
		inline bool remove_pair(const KeyValue& pair) noexcept {
			std::lock_guard<std::mutex> lock(method_mutex);
			if (!check_init_db()) return false;
			return utils::prepare(sqlite_db, "DELETE FROM '" + config.table + "' WHERE key == " + std::to_string(pair.key));
		}

		/** \brief Удалить значения по ключам
		 * \param keys	Массив ключей пар "ключ-значение"
		 */
		template<class T>
		inline bool remove_values(const T &keys) noexcept {
			std::lock_guard<std::mutex> lock(method_mutex);
			if (!check_init_db()) return false;
			if (keys.empty()) return false;
			std::string message("DELETE FROM '" + config.table + "' WHERE key IN (");
			for (auto it = keys.begin(); it != keys.end(); ++it) {
				if (it != keys.begin()) message += ",";
				message += std::to_string(*it);
			}
			message += ")";
			return utils::prepare(sqlite_db, message);
		}

		/** \brief Удалить пары "ключ-значение" по ключам
		 * \param pairs		Массив пар "ключ-значение"
		 */
		template<class T>
		inline bool remove_pairs(const T &pairs) noexcept {
			std::lock_guard<std::mutex> lock(method_mutex);
			if (!check_init_db()) return false;
			if (pairs.empty()) return false;
			std::string message("DELETE FROM '" + config.table + "' WHERE key IN (");
			for (auto it = pairs.begin(); it != pairs.end(); ++it) {
				if (it != pairs.begin()) message += ",";
				message += std::to_string(it->key);
			}
			message += ")";
			return utils::prepare(sqlite_db, message);
		}
	};

}; // trading_db

#endif // TRADING_DB_KEY_VALUE_DATABASE_HPP_INCLUDED
