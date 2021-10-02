#pragma once
#ifndef TRADING_DB_LIST_DATABASE_HPP_INCLUDED
#define TRADING_DB_LIST_DATABASE_HPP_INCLUDED

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

    /** \brief Класс базы данных списка
     */
	class ListDatabase {
    public:

        /** \brief Элемент списка
         */
        class Item {
        public:
            int64_t key = 0;
            std::string value;

            Item() {};

            Item(const int64_t k, const std::string &v) : key(k), value(v) {};
        };

	private:

        /** \brief Класс конфигурации базы данных
         */
        class Config {
        public:
            const std::string title = "trading_db::ListDatabase ";
            int busy_timeout = 0;
            std::atomic<bool> use_log = ATOMIC_VAR_INIT(false);
        };

        Config config;

        std::string database_name;
        sqlite3 *sqlite_db = nullptr;
        // команды для транзакций
        utility::SqliteTransaction sqlite_transaction;
        // предкомпилированные команды
        utility::SqliteStmt stmt_replace_item;
        utility::SqliteStmt stmt_replace_items;
        utility::SqliteStmt stmt_insert_value;
        utility::SqliteStmt stmt_get_value;
        utility::SqliteStmt stmt_get_all_items;
        std::mutex stmt_mutex;
        // бэкап
		bool is_backup = ATOMIC_VAR_INIT(false);
		std::mutex backup_mutex;
		// флаг сброса
		std::atomic<bool> is_shutdown = ATOMIC_VAR_INIT(false);

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

        /** \brief Открыть БД
         */
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
            }

            sqlite3_busy_timeout(sqlite_db_ptr, config.busy_timeout);
            // создаем таблицу в базе данных, если она еще не создана
            const char *create_list_table_sql =
                "CREATE TABLE IF NOT EXISTS 'List' ("
                "key                INTEGER PRIMARY KEY NOT NULL,"
                "value              TEXT                NOT NULL)";
            if (!utility::prepare(sqlite_db_ptr, create_list_table_sql)) return false;
            return true;
        }

        /** \brief Инициализировать БД
         */
        bool init_db(const std::string &db_name, const bool readonly = false) {
            if (!open_db(this->sqlite_db, db_name, readonly)) {
                sqlite3_close_v2(this->sqlite_db);
                this->sqlite_db = nullptr;
                return false;
            }

            sqlite_transaction.init(this->sqlite_db);
            stmt_replace_item.init(this->sqlite_db, "INSERT OR REPLACE INTO 'List' (key, value) VALUES (?, ?)");
            stmt_insert_value.init(this->sqlite_db, "INSERT INTO 'List' (value) VALUES (?)");
            stmt_get_value.init(this->sqlite_db, "SELECT value FROM 'List' WHERE key == :x");
            stmt_get_all_items.init(this->sqlite_db, "SELECT * FROM 'List'");
            return true;
        }

		template<class T>
		bool replace_or_insert(T &item, utility::SqliteTransaction &transaction, utility::SqliteStmt &stmt) {
            if (!transaction.begin_transaction()) return false;
            sqlite3_reset(stmt.get());
            if (item.key <= 0) {
                if (sqlite3_bind_text(stmt.get(), 1, item.value.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
                    transaction.rollback();
                    return false;
                }
            } else {
                if (sqlite3_bind_int64(stmt.get(), 1, item.key) != SQLITE_OK) {
                    transaction.rollback();
                    return false;
                }
                if (sqlite3_bind_text(stmt.get(), 2, item.value.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
                    transaction.rollback();
                    return false;
                }
            }
            int err = sqlite3_step(stmt.get());
            sqlite3_reset(stmt.get());
            sqlite3_clear_bindings(stmt.get());
            if(err == SQLITE_DONE) {
                //...
            } else
            if(err == SQLITE_BUSY) {
                // Если оператор не является COMMIT и встречается в явной транзакции, вам следует откатить транзакцию, прежде чем продолжить
                transaction.rollback();
                print_error("sqlite3_step return SQLITE_BUSY");
                return false;
            } else {
                transaction.rollback();
                print_error(std::string(sqlite3_errmsg(sqlite_db)) + ", code " + std::to_string(err));
                return false;
            }
            if (!transaction.commit()) return false;
            if (item.key <= 0) {
                item.key = sqlite3_last_insert_rowid(sqlite_db);
            }
            return true;
        }

        template<class T>
        bool replace_buffer(const T &buffer, utility::SqliteTransaction &transaction, utility::SqliteStmt &stmt) {
            if (buffer.empty()) return true;
            if (!transaction.begin_transaction()) return false;
            sqlite3_reset(stmt.get());
            for (size_t i = 0; i < buffer.size(); ++i) {
                if (sqlite3_bind_int64(stmt.get(), 1, static_cast<sqlite3_int64>(buffer[i].key)) != SQLITE_OK) {
                    transaction.rollback();
                    return false;
                }
                if (sqlite3_bind_text(stmt.get(), 2, buffer[i].value.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
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

        bool replace_map(const std::map<int64_t, std::string> &buffer, utility::SqliteTransaction &transaction, utility::SqliteStmt &stmt) {
            if (buffer.empty()) return true;
            if (!transaction.begin_transaction()) return false;
            sqlite3_reset(stmt.get());
            for (const auto &item : buffer) {
                if (sqlite3_bind_int64(stmt.get(), 1, static_cast<sqlite3_int64>(item.first)) != SQLITE_OK) {
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

        template<class T>
		inline T get_item(utility::SqliteStmt &stmt, const int64_t key) {
            T item;
            int err = 0;
            while (true) {
                if ((err = sqlite3_reset(stmt.get())) != SQLITE_OK) {
                    print_error("sqlite3_reset return code " + std::to_string(err));
                    return T();
                }
                if (sqlite3_bind_int64(stmt.get(), 1, key) != SQLITE_OK) {
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
                item.key = key;
                item.value = (const char *)sqlite3_column_text(stmt.get(),0);
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
                    item = T();
                    print_error("sqlite3_step return SQLITE_BUSY");
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }
                break;
            }
            return std::move(item);
		}

		template<class T>
		inline T get_items(utility::SqliteStmt &stmt) {
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
                    typename T::value_type item;
                    item.key = sqlite3_column_int64(stmt.get(), 0);
                    item.value = (const char *)sqlite3_column_text(stmt.get(), 1);

                    buffer.push_back(item);
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

		inline std::map<int64_t, std::string> get_map_items(utility::SqliteStmt &stmt) {
            std::map<int64_t, std::string> buffer;
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
                    Item item;
                    item.key = sqlite3_column_int64(stmt.get(), 0);
                    item.value = (const char *)sqlite3_column_text(stmt.get(), 1);
                    buffer[item.key] = item.value;
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

	public:

        /** \brief Конструктор хранилища тиков
         * \param path  Путь к файлу
         */
		ListDatabase(const std::string &path, const bool readonly = false) :
            database_name(path) {
            if (init_db(path, readonly)) {

            }
		}

		~ListDatabase() {
            is_shutdown = true;
            async_tasks.clear();
            if (this->sqlite_db != nullptr) {
                sqlite3_close_v2(this->sqlite_db);
            }
		}

        /** \brief Создать бэкап
         * \param path      Путь к бэкапу базы данных
         * \param callback  Функция обратного вызова для отслеживания результата бэкапа
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

		/** \brief Установить элементы списока
         * \param items Элементы списка
         * \return Вернет true в случае успешного завершения
         */
        template<class T>
		inline bool set_items(const T &items) noexcept {
            while (!is_shutdown) {
                {
                    std::lock_guard<std::mutex> lock(stmt_mutex);
                    if (replace_buffer<T>(items, sqlite_transaction, stmt_replace_item)) return true;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            return false;
		}

		/** \brief Установить элементы списка из map
         * \param items Элементы списка
         * \return Вернет true в случае успешного завершения
         */
		inline bool set_map_items(const std::map<int64_t, std::string> &items) noexcept {
            while (!is_shutdown) {
                {
                    std::lock_guard<std::mutex> lock(stmt_mutex);
                    if (replace_map(items, sqlite_transaction, stmt_replace_item)) return true;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            return false;
		}

		/** \brief Установить элемент списка
         * \param key   Ключ элемента списка
         * \param value Значение элемента списка
         * \return Вернет true в случае успешного завершения
         */
		inline bool set_item(Item &item) noexcept {
            while (!is_shutdown) {
                std::lock_guard<std::mutex> lock(stmt_mutex);
                if (item.key <= 0) {
                    if (replace_or_insert(item, sqlite_transaction, stmt_insert_value)) return true;
                } else {
                    if (replace_or_insert(item, sqlite_transaction, stmt_replace_item)) return true;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            return false;
		}

		/** \brief Установить значение
         * \param value Значение элемента списка
         * \return Вернет true в случае успешного завершения
         */
		inline bool set_value(const std::string& value) noexcept {
		    Item item{0, value};
            while (!is_shutdown) {
                {
                    std::lock_guard<std::mutex> lock(stmt_mutex);
                    if (replace_or_insert(item, sqlite_transaction, stmt_insert_value)) return true;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            return false;
		}

        /** \brief Установить значение по ключу
         * \param key   Ключ элемента списка
         * \param value Значение элемента списка
         * \return Вернет true в случае успешного завершения
         */
		inline bool set_value(const int64_t key, const std::string& value) noexcept {
		    Item item{key, value};
            while (!is_shutdown) {
                {
                    std::lock_guard<std::mutex> lock(stmt_mutex);
                    if (item.key <= 0) {
                        if (replace_or_insert(item, sqlite_transaction, stmt_insert_value)) return true;
                    } else {
                        if (replace_or_insert(item, sqlite_transaction, stmt_replace_item)) return true;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            return false;
		}

		/** \brief Установить целое значение по ключу
         * \param key   Ключ элемента списка
         * \param value Целое значение элемента списка
         * \return Вернет true в случае успешного завершения
         */
		inline bool set_int64_value(const int64_t key, const int64_t value) noexcept {
		    return set_value(key, std::to_string(value));
		}

        /** \brief Получить значение по ключу
         * \param key   Ключ элемента списка
         * \return Значение элемента списка по ключу
         */
		inline std::string get_value(const int64_t key) noexcept {
			std::lock_guard<std::mutex> lock(stmt_mutex);
            return get_item<Item>(stmt_get_value, key).value;
		}

		/** \brief Получить целое значение по ключу
         * \param key   Ключ элемента списка
         * \return Значение элемента списка по ключу
         */
		inline int64_t get_int64_value(const int64_t key) noexcept {
            return std::stoll(get_value(key));
		}

        /** \brief Получить весь список пар "ключ-значение"
         * \return Массив пар "ключ-значение"
         */
        template<class T>
		inline T get_all() noexcept {
            std::lock_guard<std::mutex> lock(stmt_mutex);
            return get_items<T>(stmt_get_all_items);
		}

		/** \brief Получить весь список пар "ключ-значение" в виде map
         * \return Массив пар "ключ-значение"
         */
		std::map<int64_t, std::string> get_map_all() noexcept {
            std::lock_guard<std::mutex> lock(stmt_mutex);
            return get_map_items(stmt_get_all_items);
		}

        /** \brief Очистить все данные
         */
		inline bool remove_all() {
            std::lock_guard<std::mutex> lock(stmt_mutex);
            return utility::prepare(sqlite_db, "DELETE FROM 'List'");
		}

		/** \brief Очистить от элемента по ключу
		 * \param key   Ключ элемента списка
         */
		inline bool remove_item(const int64_t key) {
            std::lock_guard<std::mutex> lock(stmt_mutex);
            return utility::prepare(sqlite_db, "DELETE FROM 'List' WHERE key == " + std::to_string(key));
		}
	};

}; // trading_db

#endif // TRADING_DB_KEY_VALUE_DATABASE_HPP_INCLUDED
