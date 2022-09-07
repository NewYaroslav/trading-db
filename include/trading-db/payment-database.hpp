#pragma once
#ifndef TRADING_DB_PAYMENT_DATABASE_HPP
#define TRADING_DB_PAYMENT_DATABASE_HPP

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
	class PaymentDatabase {
    public:

        /** \brief Структура данных прокси
         */
        class Payment {
        public:
            std::string user_id;    	/**< ID Пользователя */
			std::string account_id; 	/**< ID аккаунта пользователя */
			double commission = 0;		/**< Комиссия, которую пользователь должен заплатить */
			double paid = 0;			/**< Сколько пользователь заплатил */
			uint64_t timestamp = 0; 	/**< Метка времени счета */
			uint32_t service_code = 0;	/**< Номер услуги */
			uint32_t promotion = 0;		/**< Акция, если есть */

            Payment() {};
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
                (SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX) :
                (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX);
            if (sqlite3_open_v2(db_name.c_str(), &sqlite_db_ptr, flags, nullptr) != SQLITE_OK) {
                TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: " << sqlite3_errmsg(sqlite_db_ptr) << ", db name " << db_name << std::endl;
                sqlite3_close_v2(sqlite_db_ptr);
                sqlite_db_ptr = nullptr;
                return false;
            } else {
                sqlite3_busy_timeout(sqlite_db_ptr, config.busy_timeout);
                // создаем таблицу в базе данных, если она еще не создана
                const char *create_proxy_table_sql =
                    "CREATE TABLE IF NOT EXISTS 'Payment' ("
					"user_id            TEXT    PRIMARY KEY NOT NULL,"
					"account_id         TEXT                NOT NULL,"
					"commission         REAL                NOT NULL,"
					"paid            	REAL                NOT NULL,"
					"timestamp          INTEGER 			NOT NULL,"
					"service_code       INTEGER 			NOT NULL,"
                    "promotion          INTEGER 			NOT NULL)";
                if (!utility::prepare(sqlite_db_ptr, create_proxy_table_sql)) return false;
                if (!utility::sqlite_exec(sqlite_db_ptr, "PRAGMA synchronous = NORMAL")) return false;
                if (!utility::sqlite_exec(sqlite_db_ptr, "PRAGMA cache_size = " + std::to_string(config.cache_size))) return false;
                if (!utility::sqlite_exec(sqlite_db_ptr, "PRAGMA temp_store = MEMORY")) return false;
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


        bool replace(std::deque<Payment> &buffer, utility::SqliteTransaction &transaction, utility::SqliteStmt &stmt) {
			if (buffer.empty()) return true;
            if (!transaction.begin_transaction()) return false;
            sqlite3_reset(stmt.get());
            for (size_t i = 0; i < buffer.size(); ++i) {
                if (sqlite3_bind_text(stmt.get(), 1, buffer[i].user_id.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
					transaction.rollback();
					return false;
				}
				if (sqlite3_bind_text(stmt.get(), 2, buffer[i].account_id.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
					transaction.rollback();
					return false;
				}
				if (sqlite3_bind_double(stmt.get(), 3, static_cast<double>(buffer[i].commission)) != SQLITE_OK) {
                    transaction.rollback();
                    return false;
                }
                if (sqlite3_bind_double(stmt.get(), 4, static_cast<double>(buffer[i].paid)) != SQLITE_OK) {
                    transaction.rollback();
                    return false;
                }
				if (sqlite3_bind_int64(stmt.get(), 5, static_cast<sqlite3_int64>(buffer[i].timestamp)) != SQLITE_OK) {
					transaction.rollback();
					return false;
				}
				if (sqlite3_bind_int32(stmt.get(), 6, static_cast<sqlite3_int32>(buffer[i].service_code)) != SQLITE_OK) {
					transaction.rollback();
					return false;
				}
				if (sqlite3_bind_int32(stmt.get(), 7, static_cast<sqlite3_int32>(buffer[i].promotion)) != SQLITE_OK) {
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

        bool replace(Payment &item, utility::SqliteTransaction &transaction, utility::SqliteStmt &stmt) {
            if (!transaction.begin_transaction()) return false;
            sqlite3_reset(stmt.get());
			if (sqlite3_bind_text(stmt.get(), 1, item.user_id.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
				transaction.rollback();
				return false;
			}
			if (sqlite3_bind_text(stmt.get(), 2, item.account_id.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
				transaction.rollback();
				return false;
			}
			if (sqlite3_bind_double(stmt.get(), 3, static_cast<double>(item.commission)) != SQLITE_OK) {
				transaction.rollback();
				return false;
			}
			if (sqlite3_bind_double(stmt.get(), 4, static_cast<double>(item.paid)) != SQLITE_OK) {
				transaction.rollback();
				return false;
			}
			if (sqlite3_bind_int64(stmt.get(), 5, static_cast<sqlite3_int64>(item.timestamp)) != SQLITE_OK) {
				transaction.rollback();
				return false;
			}
			if (sqlite3_bind_int32(stmt.get(), 6, static_cast<sqlite3_int32>(item.service_code)) != SQLITE_OK) {
				transaction.rollback();
				return false;
			}
			if (sqlite3_bind_int32(stmt.get(), 7, static_cast<sqlite3_int32>(item.promotion)) != SQLITE_OK) {
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
                        "INSERT OR REPLACE INTO 'Payment' (user_id, account_id, commission, paid, timestamp, service_code, promotion) "
                        "VALUES (?, ?, ?, ?, ?, ?, ?)";
                    stmt_replace.init(this->sqlite_db, query);
                }

                uint64_t last_tick_timestamp = 0; // метка времени последнего тика
                while (true) {
                    // данные для записи
                    std::deque<Payment> buffer;
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

		inline Payment get_payment_from_db(utility::SqliteStmt &stmt, const std::string &key) {
            Payment payment;
            int err = 0;
            while (true) {
                if ((err = sqlite3_reset(stmt.get())) != SQLITE_OK) {
                    TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: sqlite3_reset return code " << err << std::endl;
                    return Payment();
                }
                if (sqlite3_bind_text(stmt.get(), 1, key.c_str(), key.size(), SQLITE_TRANSIENT) != SQLITE_OK) {
                    TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: sqlite3_reset return code " << err << std::endl;
                    return Payment();
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
                    return Payment();
                }

				payment.user_id = (const char *)sqlite3_column_text(stmt.get(), 0);
				payment.account_id = (const char *)sqlite3_column_text(stmt.get(), 1);
                payment.commission = (const char *)sqlite3_column_text(stmt.get(), 2);
				payment.paid = (const char *)sqlite3_column_text(stmt.get(), 3);
				payment.timestamp = sqlite3_column_int64(stmt.get(), 4);
				payment.service_code = sqlite3_column_int32(stmt.get(), 6);
				payment.promotion = sqlite3_column_int32(stmt.get(), 7);

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
		PaymentDatabase(const std::string &path, const bool readonly = false) :
            database_name(path) {
            if (init_db(path, readonly)) {
                init_other();
            }
		}

		~PaymentDatabase() {
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
		void set(const Payment &payment) {
			reset_stop_counter();
			std::lock_guard<std::mutex> lock(write_buffer_mutex);
			write_buffer.push_back(payment);
		}

        /** \brief Получить данные прокси
         * \param ip    IP адрес искомых прокси
         */
		Payment get(const std::string &ip) {
			utility::SqliteStmt stmt;
			const char *query = "SELECT * FROM 'Proxy' WHERE ip == :x";
            stmt.init(this->sqlite_db, query);
			return get_payment_from_db(stmt, ip);
		}
	};
};

#endif /* TRADING_DB_PAYMENT_DATABASE_HPP */
