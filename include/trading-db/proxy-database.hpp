#pragma once
#ifndef TRADING_DB_TICK_DATABASE_HPP
#define TRADING_DB_TICK_DATABASE_HPP

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

namespace trading_db {

    /** \brief Класс базы данных прокси
     */
	class ProxyDatabase {
    public:

        /** \brief Структура данных прокси
         */
        class Proxy {
        public:
            std::string ip;         /**< IP адрес прокси */
			std::string broker;     /**< Брокер */
			std::string email;      /**< e-mail используемых прокси */
			std::string user_id;    /**< ID Пользователя */
			std::string account_id; /**< ID аккаунта пользователя */
			uint64_t timestamp = 0; /**< Последняя метка времени использования прокси */

            Proxy() {};
        };

        class Config {
        public:
            size_t cache_size = 25000;
            size_t max_buffer_size_commit = 5000;
            size_t max_buffer_autochekpont = 10;
            size_t write_buffer_size_trigger = 1000;
            size_t busy_timeout = 0;

            const size_t wait_delay = 10;
            const size_t wait_process_delay = 500;
        };

        Config config;

    private:
        std::string database_name;
        sqlite3 *sqlite_db = nullptr;

        bool open_db(sqlite3 *&sqlite_db_ptr, const std::string &db_name, const bool readonly = false) {
            int flags = readonly ?
                (SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX | SQLITE_OPEN_WAL) :
                (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX | SQLITE_OPEN_WAL);
            if (sqlite3_open_v2(db_name.c_str(), &sqlite_db_ptr, flags, nullptr) != SQLITE_OK) {
                TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: " << sqlite3_errmsg(sqlite_db_ptr) << ", db name " << db_name << std::endl;
                sqlite3_close_v2(sqlite_db_ptr);
                sqlite_db_ptr = nullptr;
                return false;
            } else {
                sqlite3_busy_timeout(sqlite_db_ptr, config.busy_timeout);
                // создаем таблицу в базе данных, если она еще не создана
                const char *create_proxy_table_sql =
                    "CREATE TABLE IF NOT EXISTS 'Proxy' ("
					"ip                	TEXT    PRIMARY KEY NOT NULL,"
					"broker             TEXT                NOT NULL,"
					"email             	TEXT                NOT NULL,"
					"user_id            TEXT                NOT NULL,"
					"account_id         TEXT                NOT NULL,"
                    "timestamp          INTEGER 			NOT NULL)";
                if (!utility::prepare(sqlite_db_ptr, create_proxy_table_sql)) return false;
                if (!utility::sqlite_exec(sqlite_db_ptr, "PRAGMA synchronous = NORMAL")) return false;
                if (!utility::sqlite_exec(sqlite_db_ptr, "PRAGMA wal_checkpoint(FULL)")) return false;
                if (!utility::sqlite_exec(sqlite_db_ptr, "PRAGMA cache_size = " + std::to_string(config.cache_size))) return false;
                if (!utility::sqlite_exec(sqlite_db_ptr, "PRAGMA temp_store = MEMORY")) return false;
                if (!utility::sqlite_exec(sqlite_db_ptr, "PRAGMA wal_autocheckpoint = 10000")) return false;
            }
            return true;
        }

        bool init_db(const std::string &db_name, const bool readonly = false) {
            // открываем и возможно еще создаем таблицу
            if (!open_db(this->sqlite_db, db_name, readonly)) {
                sqlite3_close_v2(this->sqlite_db);
                this->sqlite_db = nullptr;
                is_error = true;
                return false;
            }
            return true;
        }

        const size_t wait_delay = 10;
        const size_t wait_process_delay = 500;

        utility::AsyncTasks async_tasks;

        std::deque<Proxy> write_buffer;
        std::mutex write_buffer_mutex;

		std::atomic<bool> is_backup = ATOMIC_VAR_INIT(false);
		std::atomic<bool> is_recording = ATOMIC_VAR_INIT(false);
		std::atomic<bool> is_stop_write = ATOMIC_VAR_INIT(false);
		std::atomic<bool> is_block_write = ATOMIC_VAR_INIT(false);
		std::atomic<bool> is_restart_write = ATOMIC_VAR_INIT(false);
		std::atomic<bool> is_writing = ATOMIC_VAR_INIT(false);
		std::atomic<bool> is_shutdown = ATOMIC_VAR_INIT(false);
        std::atomic<bool> is_stop = ATOMIC_VAR_INIT(false);

        std::atomic<bool> is_error = ATOMIC_VAR_INIT(false);

        std::atomic<uint32_t> standby_time_end_ticks = ATOMIC_VAR_INIT(60);
        std::atomic<uint32_t> last_reset_timestamp = ATOMIC_VAR_INIT(0);

        /** \brief Получить размер буфера для записи
         */
		inline size_t get_write_buffer_size() noexcept {
            std::lock_guard<std::mutex> lock(write_buffer_mutex);
            return write_tick_buffer.size();
		}


        bool replace(std::deque<Proxy> &buffer, utility::SqliteTransaction &transaction, utility::SqliteStmt &stmt) {
            if (buffer.empty()) return true;
            if (!transaction.begin_transaction()) return false;
            sqlite3_reset(stmt.get());
            for (size_t i = 0; i < buffer.size(); ++i) {
                if (sqlite3_bind_text(stmt.get(), 1, buffer[i].ip.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
					transaction.rollback();
					return false;
				}
				if (sqlite3_bind_text(stmt.get(), 2, buffer[i].broker.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
					transaction.rollback();
					return false;
				}
				if (sqlite3_bind_text(stmt.get(), 3, buffer[i].email.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
					transaction.rollback();
					return false;
				}
				if (sqlite3_bind_text(stmt.get(), 4, buffer[i].user_id.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
					transaction.rollback();
					return false;
				}
				if (sqlite3_bind_text(stmt.get(), 5, buffer[i].account_id.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
					transaction.rollback();
					return false;
				}
				if (sqlite3_bind_int64(stmt.get(), 6, static_cast<sqlite3_int64>(buffer[i].timestamp)) != SQLITE_OK) {
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
                    TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: sqlite3_step return SQLITE_BUSY" << std::endl;
                    transaction.rollback();
                    return false;
                } else {
                    TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: " << sqlite3_errmsg(sqlite_db) << ", code " << err << std::endl;
                }
            }
            if (!transaction.commit()) return false;
            return true;
        }

        bool replace(Proxy &item, utility::SqliteTransaction &transaction, utility::SqliteStmt &stmt) {
            if (!transaction.begin_transaction()) return false;
            sqlite3_reset(stmt.get());
            if (sqlite3_bind_text(stmt.get(), 1, item.ip.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
                transaction.rollback();
                return false;
            }
            if (sqlite3_bind_text(stmt.get(), 2, item.broker.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
                transaction.rollback();
                return false;
            }
			if (sqlite3_bind_text(stmt.get(), 3, item.email.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
                transaction.rollback();
                return false;
            }
			if (sqlite3_bind_text(stmt.get(), 4, item.user_id.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
                transaction.rollback();
                return false;
            }
			if (sqlite3_bind_text(stmt.get(), 5, item.account_id.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
                transaction.rollback();
                return false;
            }
			if (sqlite3_bind_int64(stmt.get(), 6, static_cast<sqlite3_int64>(item.timestamp)) != SQLITE_OK) {
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
                TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: sqlite3_step return SQLITE_BUSY" << std::endl;
                // Если оператор не является COMMIT и встречается в явной транзакции, вам следует откатить транзакцию, прежде чем продолжить.
                transaction.rollback();
                return false;
            } else {
                TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: " << sqlite3_errmsg(sqlite_db) << ", code " << err << std::endl;
            }
            if (!transaction.commit()) return false;
            return true;
        }

        /** \brief Инициализация объектов класса
         */
		inline void init_other() noexcept {
            async_tasks.create_task([&]() {
                // создаем подготовленные SQL команды
                utility::SqliteTransaction sqlite_transaction;
                utility::SqliteStmt stmt_replace;

                sqlite_transaction.init(this->sqlite_db);
                {
                    const char *query =
                        "INSERT OR REPLACE INTO 'Proxy' (ip, broker, email, user_id, account_id, timestamp) "
                        "VALUES (?, ?, ?, ?, ?, ?)";
                    stmt_replace.init(this->sqlite_db, query);
                }

                uint64_t last_tick_timestamp = 0; // метка времени последнего тика
                while (true) {
                    // данные для записи
                    std::deque<Proxy> buffer;
                    {
                        // запоминаем данные для записи
                        std::lock_guard<std::mutex> lock(write_buffer_mutex);
                        if (xtime::get_timestamp() > last_reset_timestamp && is_recording) {
                            if (last_tick_timestamp != 0) end_tick_stamp_buffer.push_back(last_tick_timestamp);
                            is_stop_write = true;
                        }
                        if (write_buffer.size() > config.write_buffer_size_trigger || is_stop_write) {
                            if (!write_buffer.empty()) {
                                const size_t buffer_size = std::min(write_buffer.size(), config.wmax_buffer_size_commit);
                                buffer.resize(buffer_size);
                                buffer.assign(
                                    write_buffer.begin(),
                                    write_buffer.begin() + buffer_size);
                            }
                            if (is_stop_write) {
                                is_recording = false;
                            }
                            is_stop_write = false;
                        }
                    }

                    // записываем данные в БД
                    if (!buffer.empty()) {
                        if (!replace(buffer, sqlite_transaction, stmt_replace)) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(config.wait_delay));
                            continue;
                        }
                        std::lock_guard<std::mutex> lock(write_buffer_mutex);
                        // очищаем буфер c прокси
                        if (!write_buffer.empty() && !buffer.empty()) {
                            write_buffer.erase(
                                write_buffer.begin(),
                                write_buffer.begin() + buffer.size());
                        }
                    }

                    if (is_shutdown) break;
                    std::this_thread::sleep_for(std::chrono::milliseconds(config.wait_delay));
                }
                is_writing = false;
                is_stop = true;
            });
		}

		inline Proxy get_proxy_from_db(utility::SqliteStmt &stmt, const std::string &key) {
            Proxy proxy;
            int err = 0;
            while (true) {
                if ((err = sqlite3_reset(stmt.get())) != SQLITE_OK) {
                    TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: sqlite3_reset return code " << err << std::endl;
                    return Proxy();
                }
                if (sqlite3_bind_text(stmt.get(), 1, key.c_str(), key.size(), SQLITE_TRANSIENT) != SQLITE_OK) {
                    TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: sqlite3_reset return code " << err << std::endl;
                    return Proxy();
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
                    return Proxy();
                }

                proxy.ip = (const char *)sqlite3_column_text(stmt.get(),0);
				proxy.broker = (const char *)sqlite3_column_text(stmt.get(),1);
				proxy.email = (const char *)sqlite3_column_text(stmt.get(),2);
				proxy.user_id = (const char *)sqlite3_column_text(stmt.get(),3);
				proxy.account_id = (const char *)sqlite3_column_text(stmt.get(),4);
				proxy.timestamp = sqlite3_column_int64(stmt.get(),5);

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
                    proxy = Proxy();
                    TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: sqlite3_step return SQLITE_BUSY" << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }
                break;
            }
            return std::move(proxy);
		}

		bool backup_form_db(const std::string &path, sqlite3 *source_connection) {
            sqlite3 *dest_connection = nullptr;

            if (sqlite3_open(path.c_str(), &dest_connection) != SQLITE_OK) {
                TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: " << sqlite3_errmsg(dest_connection) << ", db name " << path << std::endl;
                sqlite3_close_v2(dest_connection);
                dest_connection = nullptr;
                return false;
            }

            sqlite3_backup *backup_db = sqlite3_backup_init(dest_connection, "main", source_connection, "main");
            if (!backup_db) {
                sqlite3_close_v2(dest_connection);
                return false;
            }

            while (true) {
                int err = sqlite3_backup_step(backup_db, -1);
                bool is_break = false;
                switch(err) {
                case SQLITE_DONE:
                    //continue;
                    is_break = true;
                    break;
                case SQLITE_OK:
                case SQLITE_BUSY:
                case SQLITE_LOCKED:
                    //TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: sqlite3_backup_step return code " << err << std::endl;
                    break;
                default:
                    TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: sqlite3_backup_step return code " << err << std::endl;
                    is_break = true;
                    break;
                }
                if (is_break) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }

            if (sqlite3_backup_finish(backup_db) != SQLITE_OK) {
                sqlite3_close_v2(dest_connection);
                return false;
            }
            sqlite3_close_v2(dest_connection);
            return true;
		}

		inline void reset_write_counter() {

		}

	public:

        /** \brief Конструктор хранилища тиков
         * \param path 		Путь к файлу
		 * \param readonly	Флаг "только чтение"
         */
		ProxyDatabase(const std::string &path, const bool readonly = false) :
            database_name(path) {
            if (init_db(path, readonly)) {
                init_other();
            }
		}

		~ProxyDatabase() {
            is_block_write = true;
            // ставим флаг остановки записи
            {
                std::lock_guard<std::mutex> lock(write_buffer_mutex);
                is_stop_write = true;
            }
            wait();
            is_shutdown = true;
            // ждем завершение потоков
            while (!is_stop) {
                std::this_thread::sleep_for(std::chrono::milliseconds(wait_delay));
            }
            async_tasks.clear();
            if (this->sqlite_db != nullptr) {
                sqlite3_close_v2(this->sqlite_db);
            }
		}

		inline void wait() noexcept {
            while (true) {
                {
                    std::lock_guard<std::mutex> lock(write_buffer_mutex);
                    if (write_tick_buffer.empty() && write_end_tick_stamp_buffer.empty()) {
                        return;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(wait_delay));
            }
		}

		bool backup(const std::string &path) {
            if (is_backup) return false;
            is_backup = true;
            async_tasks.create_task([&, path]() {
                if (!backup_form_db(path, this->sqlite_db)) {
                    TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: backup return false" << std::endl;
                } else {
                    TRADING_DB_TICK_DB_PRINT << path << " backup completed" << std::endl;
                }
                is_backup = false;
            });
            return true;
		}

        /** \brief Установить данные прокси
         * \param proxy Данные прокси
         */
		void set(const Proxy &proxy) {
			reset_stop_counter();
			std::lock_guard<std::mutex> lock(write_buffer_mutex);
			write_buffer.push_back(proxy);
		}

        /** \brief Получить данные прокси
         * \param ip    IP адрес искомых прокси
         */
		Proxy get(const std::string &ip) {
			utility::SqliteStmt stmt;
			const char *query = "SELECT * FROM 'Proxy' WHERE ip == :x";
            stmt.init(this->sqlite_db, query);
			return get_proxy_from_db(stmt, ip);
		}
	};
};

#endif /* TRADING_TICK_DB_HPP */
