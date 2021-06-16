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
#include <xtime.hpp>
#include <mutex>
#include <atomic>
#include <future>
#include <vector>

namespace trading_db {

    /** \brief Класс базы данных тика
     */
	class KeyValueDatabase {
    public:

        class Config {
        public:
            int busy_timeout = 0;
            std::atomic<bool> use_log = ATOMIC_VAR_INIT(false);
        };

        Config config;

        /** \brief Структура настроек
         */
        class KeyValue {
        public:
            std::string key;
            std::string value;
        };

    private:
        std::string database_name;
        sqlite3 *sqlite_db = nullptr;
        utility::SqliteTransaction sqlite_transaction;
        utility::SqliteStmt stmt_replace_key_value;
        utility::SqliteStmt stmt_get_key_value;
        utility::SqliteStmt stmt_get_all_key_value;

        std::mutex stmt_mutex;

        utility::AsyncTasks async_tasks;

		bool is_backup = ATOMIC_VAR_INIT(false);
		std::mutex backup_mutex;

        bool replace(KeyValue &item, utility::SqliteTransaction &transaction, utility::SqliteStmt &stmt) {
            if (!transaction.begin_transaction()) return false;
            sqlite3_reset(stmt.get());
            if (sqlite3_bind_text(stmt.get(), 1, item.key.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
                transaction.rollback();
                return false;
            }
            if (sqlite3_bind_text(stmt.get(), 2, item.value.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
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
                if (config.use_log) {
                    TRADING_DB_TICK_DB_PRINT
                        << "trading_db::KeyValueDatabase error in [file " << __FILE__
                        << ", line " << __LINE__
                        << ", func " << __FUNCTION__
                        << "], message: sqlite3_step return SQLITE_BUSY" << std::endl;
                }
                // Если оператор не является COMMIT и встречается в явной транзакции, вам следует откатить транзакцию, прежде чем продолжить.
                transaction.rollback();
                return false;
            } else {
                if (config.use_log) {
                    TRADING_DB_TICK_DB_PRINT
                        << "trading_db::KeyValueDatabase error in [file " << __FILE__
                        << ", line " << __LINE__
                        << ", func " << __FUNCTION__
                        << "], message: " << sqlite3_errmsg(sqlite_db)
                        << ", code " << err << std::endl;
                }
            }
            if (!transaction.commit()) return false;
            return true;
        }

        bool open_db(sqlite3 *&sqlite_db_ptr, const std::string &db_name, const bool readonly = false) {
            // открываем и возможно еще создаем таблицу
            int flags = readonly ?
                (SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX) :
                (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX);
            if(sqlite3_open_v2(db_name.c_str(), &sqlite_db_ptr, flags, nullptr) != SQLITE_OK) {
                if (config.use_log) {
                    TRADING_DB_TICK_DB_PRINT
                        << "trading_db::KeyValueDatabase error in [file " << __FILE__
                        << ", line " << __LINE__
                        << ", func " << __FUNCTION__
                        << "], message: " << sqlite3_errmsg(sqlite_db_ptr)
                        << ", db name " << db_name << std::endl;
                }
                sqlite3_close_v2(sqlite_db_ptr);
                sqlite_db_ptr = nullptr;
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
            if (!open_db(this->sqlite_db, db_name, readonly)) {
                sqlite3_close_v2(this->sqlite_db);
                this->sqlite_db = nullptr;
                return false;
            } else {
                sqlite_transaction.init(this->sqlite_db);
                stmt_replace_key_value.init(this->sqlite_db, "INSERT OR REPLACE INTO 'KeyValue' (key, value) VALUES (?, ?)");
                stmt_get_key_value.init(this->sqlite_db, "SELECT * FROM 'KeyValue' WHERE key == :x");
                stmt_get_all_key_value.init(this->sqlite_db, "SELECT * FROM 'KeyValue'");
            }
            return true;
        }

		inline KeyValue get_value_from_db(utility::SqliteStmt &stmt, const std::string &key) {
            KeyValue key_value;
            int err = 0;
            while (true) {
                if ((err = sqlite3_reset(stmt.get())) != SQLITE_OK) {
                    if (config.use_log) {
                        TRADING_DB_TICK_DB_PRINT
                            << "trading_db::KeyValueDatabase error in [file " << __FILE__
                            << ", line " << __LINE__
                            << ", func " << __FUNCTION__
                            << "], message: sqlite3_reset return code " << err << std::endl;
                    }
                    return KeyValue();
                }
                if (sqlite3_bind_text(stmt.get(), 1, key.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
                    if (config.use_log) {
                        TRADING_DB_TICK_DB_PRINT
                            << "trading_db::KeyValueDatabase error in [file " << __FILE__
                            << ", line " << __LINE__
                            << ", func " << __FUNCTION__
                            << "], message: sqlite3_reset return code " << err << std::endl;
                    }
                    return KeyValue();
                }
                err = sqlite3_step(stmt.get());
                if(err == SQLITE_BUSY) {
                    sqlite3_reset(stmt.get());
                    sqlite3_clear_bindings(stmt.get());
                    //TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: sqlite3_step return code " << err << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                } else
                if (err != SQLITE_ROW) {
                    sqlite3_reset(stmt.get());
                    sqlite3_clear_bindings(stmt.get());
                    //TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: sqlite3_step return code " << err << std::endl;
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
                    if (config.use_log) {
                        TRADING_DB_TICK_DB_PRINT
                            << "trading_db::KeyValueDatabase error in [file " << __FILE__
                            << ", line " << __LINE__
                            << ", func " << __FUNCTION__
                            << "], message: sqlite3_step return SQLITE_BUSY" << std::endl;
                    }
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
                    if (config.use_log) {
                        TRADING_DB_TICK_DB_PRINT
                            << "trading_db::KeyValueDatabase error in [file " << __FILE__
                            << ", line " << __LINE__
                            << ", func " << __FUNCTION__
                            << "], message: sqlite3_reset return code " << err << std::endl;
                    }
                    return T();
                }
                err = sqlite3_step(stmt.get());
                if(err == SQLITE_BUSY) {
                    sqlite3_reset(stmt.get());
                    sqlite3_clear_bindings(stmt.get());
                    //TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: sqlite3_step return code " << err << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                } else
                if (err != SQLITE_ROW) {
                    sqlite3_reset(stmt.get());
                    sqlite3_clear_bindings(stmt.get());
                    //TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: sqlite3_step return code " << err << std::endl;
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
                        if (config.use_log) {
                            TRADING_DB_TICK_DB_PRINT
                                << "trading_db::KeyValueDatabase error in [file " << __FILE__
                                << ", line " << __LINE__
                                << ", func " << __FUNCTION__
                                << "], message: sqlite3_step return SQLITE_BUSY" << std::endl;
                        }
                        break;
                    }
                }
                if(err == SQLITE_BUSY) {
                    sqlite3_reset(stmt.get());
                    sqlite3_clear_bindings(stmt.get());
                    buffer.clear();
                    if (config.use_log) {
                        TRADING_DB_TICK_DB_PRINT
                            << "trading_db::KeyValueDatabase error in [file " << __FILE__
                            << ", line " << __LINE__
                            << ", func " << __FUNCTION__
                            << "], message: sqlite3_step return SQLITE_BUSY" << std::endl;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }
                break;
            }
            return std::move(buffer);
		}

	public:

        /** \brief Конструктор хранилища тиков
         * \param path  Путь к файлу
         */
		KeyValueDatabase(const std::string &path, const bool readonly = false) :
            database_name(path) {
            if (init_db(path, readonly)) {

            }
		}

		~KeyValueDatabase() {
            async_tasks.clear();
            if (this->sqlite_db != nullptr) {
                sqlite3_close_v2(this->sqlite_db);
            }
		}

        /** \brief Создать бэкап
         * \param path  Путь к бэкапу базы данных
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
                    if (config.use_log) {
                        TRADING_DB_TICK_DB_PRINT
                            << "trading_db::KeyValueDatabase error in [file "
                            << __FILE__ << ", line "
                            << __LINE__ << ", func "
                            << __FUNCTION__ << "], message: backup return false" << std::endl;
                    }
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

        /** \brief Установить значение по ключу
         * \param key   Ключ заметки
         * \param value Значение заметки
         * \return Вернет true в случае успешного завершения
         */
		inline bool set_value(const std::string& key, const std::string& value) noexcept {
		    KeyValue kv{key, value};
            while (true) {
                std::lock_guard<std::mutex> lock(stmt_mutex);
                if (replace(kv, sqlite_transaction, stmt_replace_key_value)) return true;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            return false;
		}

		/** \brief Установить целое значение по ключу
         * \param key   Ключ заметки
         * \param value Целое значение заметки
         * \return Вернет true в случае успешного завершения
         */
		inline bool set_int64_value(const std::string& key, const int64_t value) noexcept {
		    return set_value(key, std::to_string(value));
		}

        /** \brief Получить значение по ключу
         * \param key   Ключ заметки
         * \return Значение по ключу
         */
		inline std::string get_value(const std::string& key) noexcept {
			std::lock_guard<std::mutex> lock(stmt_mutex);
            return get_value_from_db(stmt_get_key_value, key).value;
		}

		/** \brief Получить целое значение по ключу
         * \param key   Ключ заметки
         * \return Значение по ключу
         */
		inline int64_t get_int64_value(const std::string& key) noexcept {
            return std::stoll(get_value(key));
		}

        /** \brief Получить весь список пар "ключ-значение"
         * \return Массив пар "ключ-значение"
         */
		inline std::vector<KeyValue> get_all() noexcept {
            std::lock_guard<std::mutex> lock(stmt_mutex);
            return get_values_from_db<std::vector<KeyValue>>(stmt_get_all_key_value);
		}

        /** \brief Очистить все данные
         */
		inline bool remove_all() {
            std::lock_guard<std::mutex> lock(stmt_mutex);
            return utility::prepare(sqlite_db, "DELETE FROM 'KeyValue'");
		}
	};

}; // trading_db

#endif // TRADING_DB_KEY_VALUE_DATABASE_HPP_INCLUDED
