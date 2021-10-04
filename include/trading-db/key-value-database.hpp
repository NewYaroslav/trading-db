#pragma once
#ifndef TRADING_DB_KEY_VALUE_DATABASE_HPP_INCLUDED
#define TRADING_DB_KEY_VALUE_DATABASE_HPP_INCLUDED

#if SQLITE_THREADSAFE != 1
#error "The project must be built for sqlite multithreading! Set the SQLITE_THREADSAFE=1"
#endif

#include "config.hpp"
#include "utility/sqlite-func.hpp"
#include "utility/async-tasks.hpp"
#include "utility/print.hpp"
#include "utility/files.hpp"
#include <mutex>
#include <atomic>
#include <future>
#include <vector>
#include <map>

namespace trading_db {

    /** \brief Класс базы данных типа Ключ-Значение
     */
	class KeyValueDatabase {
    public:

        /** \brief Структура Ключ-Значение
         */
        class KeyValue {
        public:
            std::string key;
            std::string value;
        };

        /** \brief Класс конфигурации базы данных
         */
        class Config {
        public:
            const std::string title = "trading_db::KeyValueDatabase ";
            int busy_timeout = 0;
            std::atomic<bool> use_log = ATOMIC_VAR_INIT(false);
        };

        Config config;  /**< Конфигурация базы данных */

    private:

        std::string database_name;
        sqlite3 *sqlite_db = nullptr;
        // команды для транзакций
        utility::SqliteTransaction sqlite_transaction;
        // предкомпилированные команды
        utility::SqliteStmt stmt_replace_key_value;
        utility::SqliteStmt stmt_get_key_value;
        utility::SqliteStmt stmt_get_all_key_value;
        // флаг сброса
		bool is_backup = ATOMIC_VAR_INIT(false);
		std::mutex backup_mutex;
		// флаг сброса
		std::atomic<bool> is_shutdown = ATOMIC_VAR_INIT(false);

		std::mutex method_mutex;

		utility::AsyncTasks async_tasks;

		inline void print_error(const std::string message) noexcept {
            if (config.use_log) {
                TRADING_DB_TICK_DB_PRINT
                    << config.title << "error in [file " << __FILE__
                    << ", line " << __LINE__
                    << ", func " << __FUNCTION__
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
                sqlite3_close_v2(sqlite_db_ptr);
                sqlite_db_ptr = nullptr;
                print_error(std::string(sqlite3_errmsg(sqlite_db_ptr)) + ", db name " + db_name);
                return false;
            } else {
                sqlite3_busy_timeout(sqlite_db_ptr, config.busy_timeout);
                // создаем таблицу в базе данных, если она еще не создана
                const char *create_key_value_table_sql =
                    "CREATE TABLE IF NOT EXISTS 'KeyValue' ("
                    "key                TEXT    PRIMARY KEY NOT NULL,"
                    "value              TEXT                NOT NULL)";

                if (!utility::prepare(sqlite_db_ptr, create_key_value_table_sql)) return false;
            }
            return true;
        }

        bool init_db(const std::string &db_name, const bool readonly = false) {
            if (!open_db(sqlite_db, db_name, readonly)) {
                sqlite3_close_v2(sqlite_db);
                sqlite_db = nullptr;
                return false;
            } else {
                if (!sqlite_transaction.init(sqlite_db) ||
                    !stmt_replace_key_value.init(sqlite_db, "INSERT OR REPLACE INTO 'KeyValue' (key, value) VALUES (?, ?)") ||
                    !stmt_get_key_value.init(sqlite_db, "SELECT * FROM 'KeyValue' WHERE key == :x") ||
                    !stmt_get_all_key_value.init(sqlite_db, "SELECT * FROM 'KeyValue'")) {
                    sqlite3_close_v2(sqlite_db);
                    sqlite_db = nullptr;
                    return false;
                }
            }
            return true;
        }

		template<class T>
        bool replace(T &pair, utility::SqliteTransaction &transaction, utility::SqliteStmt &stmt) {
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
                print_error("sqlite3_step return SQLITE_BUSY");
                return false;
            } else {
                transaction.rollback();
                print_error(std::string(sqlite3_errmsg(sqlite_db)) + ", code " + std::to_string(err));
                return false;
            }
            if (!transaction.commit()) return false;
            return true;
        }

        bool replace_map(
                const std::map<std::string, std::string> &buffer,
                utility::SqliteTransaction &transaction,
                utility::SqliteStmt &stmt) {
            if (buffer.empty()) return true;
            if (!transaction.begin_transaction()) return false;
            sqlite3_reset(stmt.get());
            for (const auto &item : buffer) {
                if (sqlite3_bind_text(stmt.get(), 1, item.first.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
                    transaction.rollback();
                    return false;
                }
                if (sqlite3_bind_text(stmt.get(), 2, item.second.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
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
                    print_error("sqlite3_step return SQLITE_BUSY");
                    return false;
                } else {
                    transaction.rollback();
                    print_error(std::string(sqlite3_errmsg(sqlite_db)) + ", code " + std::to_string(err));
                    return false;
                }
            }
            if (!transaction.commit()) return false;
            return true;
        }

		inline KeyValue get_value_from_db(utility::SqliteStmt &stmt, const std::string &key) {
            KeyValue key_value;
            int err = 0;
            while (true) {
                if ((err = sqlite3_reset(stmt.get())) != SQLITE_OK) {
                    print_error("sqlite3_reset return code " + std::to_string(err));
                    return KeyValue();
                }
                if (sqlite3_bind_text(stmt.get(), 1, key.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
                    print_error("sqlite3_bind_text error");
                    return KeyValue();
                }
                err = sqlite3_step(stmt.get());
                if(err == SQLITE_BUSY) {
                    sqlite3_reset(stmt.get());
                    sqlite3_clear_bindings(stmt.get());
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                } else
                if (err != SQLITE_ROW) {
                    sqlite3_reset(stmt.get());
                    sqlite3_clear_bindings(stmt.get());
                    return KeyValue();
                }

                key_value.key = (const char *)sqlite3_column_text(stmt.get(),0);
                key_value.value = (const char *)sqlite3_column_text(stmt.get(),1);
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
                    key_value = KeyValue();
                    print_error("sqlite3_step return SQLITE_BUSY");
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }
                break;
            }
            return std::move(key_value);
		}

		template<class T>
		inline T get_values_from_db(utility::SqliteStmt &stmt) {
            T buffer;
            int err = 0;
            while (true) {
                if ((err = sqlite3_reset(stmt.get())) != SQLITE_OK) {
                    print_error("sqlite3_reset return code " + std::to_string(err));
                    return T();
                }
                err = sqlite3_step(stmt.get());
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
                    KeyValue key_value;
                    key_value.key = (const char *)sqlite3_column_text(stmt.get(),0);
                    key_value.value = (const char *)sqlite3_column_text(stmt.get(),1);
                    buffer.push_back(key_value);
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
                    print_error("sqlite3_step return SQLITE_BUSY");
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }
                break;
            }
            return std::move(buffer);
		}

		inline std::map<std::string, std::string> get_map_pairs(utility::SqliteStmt &stmt) {
            std::map<std::string, std::string> buffer;
            int err = 0;
            while (true) {
                if ((err = sqlite3_reset(stmt.get())) != SQLITE_OK) {
                    print_error("sqlite3_reset return code " + std::to_string(err));
                    return std::move(buffer);
                }
                err = sqlite3_step(stmt.get());
                if(err == SQLITE_BUSY) {
                    sqlite3_reset(stmt.get());
                    sqlite3_clear_bindings(stmt.get());
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                } else
                if (err != SQLITE_ROW) {
                    sqlite3_reset(stmt.get());
                    sqlite3_clear_bindings(stmt.get());
                    return std::move(buffer);
                }

                err = 0;
                while (true) {
                    KeyValue key_value;
                    key_value.key = (const char *)sqlite3_column_text(stmt.get(), 0);
                    key_value.value = (const char *)sqlite3_column_text(stmt.get(), 1);
                    buffer[key_value.key] = key_value.value;
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
                        print_error("sqlite3_step return SQLITE_BUSY");
                        break;
                    }
                }
                if(err == SQLITE_BUSY) {
                    sqlite3_reset(stmt.get());
                    sqlite3_clear_bindings(stmt.get());
                    buffer.clear();
                    print_error("sqlite3_step return SQLITE_BUSY");
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

        KeyValueDatabase() {};

        /** \brief Конструктор хранилища пар "ключ-значение"
         * \param path  Путь к файлу
         * \param readonly  Флаг "только чтение"
         * \param use_log   Включить вывод логов
         */
		KeyValueDatabase(
                const std::string &path,
                const bool readonly = false,
                const bool use_log = false) :
            database_name(path) {
            init_db(path, readonly);
            config.use_log = use_log;
		}

		~KeyValueDatabase() {
            is_shutdown = true;
            async_tasks.clear();
            std::lock_guard<std::mutex> lock(method_mutex);
            if (sqlite_db != nullptr) sqlite3_close_v2(sqlite_db);
		}

		/** \brief Инициализировать хранилище пар "ключ-значение"
         * \param path  Путь к файлу
         * \param readonly  Флаг "только чтение"
         * \param use_log   Включить вывод логов
         */
		bool init(
                const std::string &path,
                const bool readonly = false,
                const bool use_log = false) noexcept {
            std::lock_guard<std::mutex> lock(method_mutex);
            if (!check_init_db()) return false;
            database_name = path;
            init_db(path, readonly);
            config.use_log = use_log;
            return true;
		}

        /** \brief Создать бэкап
         * \param path  Путь к бэкапу базы данных
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
                if (!utility::backup_form_db(path, this->sqlite_db)) {
                    callback(path, true);
                    print_error("backup return false");
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
         * \param key   Ключ пары "ключ-значение"
         * \param value Значение пары "ключ-значение"
         * \return Вернет true в случае успешного завершения
         */
		inline bool set_pair(const std::string& key, const std::string& value) noexcept {
		    {
                std::lock_guard<std::mutex> lock(method_mutex);
                if (!check_init_db()) return false;
		    }
		    KeyValue kv{key, value};
            while (!is_shutdown) {
                {
                    std::lock_guard<std::mutex> lock(method_mutex);
                    if (replace(kv, sqlite_transaction, stmt_replace_key_value)) return true;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            return false;
		}

		/** \brief Установить пару "ключ-значение"
         * \param value Объект "ключ-значение"
         * \return Вернет true в случае успешного завершения
         */
        template<class T>
		inline bool set_pair(const T& value) noexcept {
            {
                std::lock_guard<std::mutex> lock(method_mutex);
                if (!check_init_db()) return false;
		    }
            while (!is_shutdown) {
                {
                    std::lock_guard<std::mutex> lock(method_mutex);
                    if (replace(value, sqlite_transaction, stmt_replace_key_value)) return true;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            return false;
		}

		/** \brief Установить пары "ключ-значение" из map
         * \param pairs Массив пар "ключ-значение"
         * \return Вернет true в случае успешного завершения
         */
		inline bool set_map_pairs(const std::map<std::string, std::string> &pairs) noexcept {
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

		/** \brief Установить пару "ключ-значение" с целочисленным значением
         * \param key   Ключ пары "ключ-значение"
         * \param value Целое значение пары "ключ-значение"
         * \return Вернет true в случае успешного завершения
         */
		inline bool set_pair_int64_value(const std::string& key, const int64_t value) noexcept {
		    return set_pair(key, std::to_string(value));
		}

        /** \brief Получить значение по ключу
         * \param key   Ключ заметки
         * \return Значение по ключу
         */
		inline std::string get_value(const std::string& key) noexcept {
			std::lock_guard<std::mutex> lock(method_mutex);
			if (!check_init_db()) return std::string();
            return get_value_from_db(stmt_get_key_value, key).value;
		}

		/** \brief Получить пару "ключ-значение" по ключу
         * \param key   Ключ пары "ключ-значение"
         * \return Пара "ключ-значение"
         */
		inline KeyValue get_pair(const std::string& key) noexcept {
			std::lock_guard<std::mutex> lock(method_mutex);
			if (!check_init_db()) return KeyValue();
            return get_value_from_db(stmt_get_key_value, key);
		}

		/** \brief Получить целое значение по ключу
         * \param key   Ключ пары "ключ-значение"
         * \return Значение по ключу
         */
		inline int64_t get_int64_value(const std::string& key) noexcept {
            return std::stoll(get_value(key));
		}

        /** \brief Получить весь список пар "ключ-значение"
         * \return Массив пар "ключ-значение"
         */
        template<class T>
		inline T get_all_pairs() noexcept {
            std::lock_guard<std::mutex> lock(method_mutex);
            if (!check_init_db()) return T();
            return get_values_from_db<T>(stmt_get_all_key_value);
		}

		/** \brief Получить весь список пар "ключ-значение" в виде map
         * \return Массив пар "ключ-значение"
         */
		std::map<std::string, std::string> get_map_all_pairs() noexcept {
            std::lock_guard<std::mutex> lock(method_mutex);
            if (!check_init_db()) return std::map<std::string, std::string>();
            return get_map_pairs(stmt_get_all_key_value);
		}

        /** \brief Удалить все данные
         */
		inline bool remove_all() noexcept {
            std::lock_guard<std::mutex> lock(method_mutex);
            if (!check_init_db()) return false;
            return utility::prepare(sqlite_db, "DELETE FROM 'KeyValue'");
		}

		/** \brief Удалить значение по ключу
		 * \param key   Ключ пары "ключ-значение"
         */
		inline bool remove_value(const std::string& key) noexcept {
            std::lock_guard<std::mutex> lock(method_mutex);
            if (!check_init_db()) return false;
            return utility::prepare(sqlite_db, "DELETE FROM 'KeyValue' WHERE key == " + key);
		}

		/** \brief Удалить пару "ключ-значение"
		 * \param pair  Пары "ключ-значение"
         */
		inline bool remove_pair(const KeyValue& pair) noexcept {
            std::lock_guard<std::mutex> lock(method_mutex);
            if (!check_init_db()) return false;
            return utility::prepare(sqlite_db, "DELETE FROM 'KeyValue' WHERE key == " + pair.key);
		}

		/** \brief Удалить значения по ключам
		 * \param keys  Массив ключей пар "ключ-значение"
         */
        template<class T>
		inline bool remove_values(const T &keys) noexcept {
            std::lock_guard<std::mutex> lock(method_mutex);
            if (!check_init_db()) return false;
            if (keys.empty()) return false;
            std::string message("DELETE FROM 'KeyValue' WHERE key IN (");
            for (auto it = keys.begin(); it != keys.end(); ++it) {
                if (it != keys.begin()) message += ",";
                message += "'";
                message += *it;
                message += "'";
            }
            message += ")";
            return utility::prepare(sqlite_db, message);
		}

		/** \brief Удалить пары "ключ-значение" по ключам
		 * \param pairs     Массив пар "ключ-значение"
         */
        template<class T>
		inline bool remove_pairs(const T &pairs) noexcept {
            std::lock_guard<std::mutex> lock(method_mutex);
            if (!check_init_db()) return false;
            if (pairs.empty()) return false;
            std::string message("DELETE FROM 'KeyValue' WHERE key IN (");
            for (auto it = pairs.begin(); it != pairs.end(); ++it) {
                if (it != pairs.begin()) message += ",";
                message += "'";
                message += it->key;
                message += "'";
            }
            message += ")";
            return utility::prepare(sqlite_db, message);
		}
	};

}; // trading_db

#endif // TRADING_DB_KEY_VALUE_DATABASE_HPP_INCLUDED
