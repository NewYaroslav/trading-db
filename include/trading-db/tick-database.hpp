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

    /** \brief Класс базы данных тика
     */
	class TickDatabase {
    public:

        /** \brief Структура данных одного тика
         */
        class Tick {
        public:
            uint64_t timestamp = 0;
            uint64_t server_timestamp = 0;
            double bid = 0;
            double ask = 0;

            Tick(
                const uint64_t t,
                const uint64_t st,
                const double b,
                const double a) :
                timestamp(t),
                server_timestamp(st),
                bid(b),
                ask(a) {
            }

            Tick() {};

            bool empty() const noexcept {
                return (timestamp == 0);
            }
        };

        /** \brief Структура настроек
         */
        class Note {
        public:
            std::string key;
            std::string value;
        };

    private:
        std::string database_name;
        sqlite3 *sqlite_db = nullptr;
        utility::SqliteTransaction sqlite_transaction;
        utility::SqliteStmt stmt_get_first_ticks_lower;
        utility::SqliteStmt stmt_get_first_ticks_upper;
        utility::SqliteStmt stmt_get_first_ticks_lower_2;
        utility::SqliteStmt stmt_get_first_ticks_upper_2;
        utility::SqliteStmt stmt_get_first_end_tick_lower;
        utility::SqliteStmt stmt_get_first_end_tick_upper;
        utility::SqliteStmt stmt_replace_note;
        utility::SqliteStmt stmt_get_note;
		utility::SqliteStmt stmt_get_max;
        utility::SqliteStmt stmt_get_min;

        std::mutex stmt_mutex;

        size_t sqlite_cache_size = 25000;
        size_t max_buffer_size_commit = 5000;
        size_t max_buffer_autochekpont = 10;
        size_t write_buffer_size_trigger = 1000;
        size_t busy_timeout = 0;

        bool exec_db(const std::string &sql_statement) {
            char *err = nullptr;
            if(sqlite3_exec(this->sqlite_db, sql_statement.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
                TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: " << err << std::endl;
                sqlite3_free(err);
                //sqlite3_close(this->sqlite_db);
                //this->sqlite_db = nullptr;
                is_error = true;
                return false;
            }
            return true;
        }

        bool exec_db(sqlite3 *sqlite_db_ptr, const std::string &sql_statement) {
            char *err = nullptr;
            if(sqlite3_exec(sqlite_db_ptr, sql_statement.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
                TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: " << err << std::endl;
                sqlite3_free(err);
                return false;
            }
            return true;
        }

        bool replace(std::deque<Tick> &buffer, utility::SqliteTransaction &transaction, utility::SqliteStmt &stmt) {
            if (buffer.empty()) return true;
            if (!transaction.begin_transaction()) return false;
            sqlite3_reset(stmt.get());
            for (size_t i = 0; i < buffer.size(); ++i) {
                if (sqlite3_bind_int64(stmt.get(), 1, static_cast<sqlite3_int64>(buffer[i].timestamp)) != SQLITE_OK) {
                    transaction.rollback();
                    return false;
                }
                if (sqlite3_bind_int64(stmt.get(), 2, static_cast<sqlite3_int64>(buffer[i].server_timestamp)) != SQLITE_OK) {
                    transaction.rollback();
                    return false;
                }
                if (sqlite3_bind_double(stmt.get(), 3, static_cast<double>(buffer[i].bid)) != SQLITE_OK) {
                    transaction.rollback();
                    return false;
                }
                if (sqlite3_bind_double(stmt.get(), 4, static_cast<double>(buffer[i].ask)) != SQLITE_OK) {
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
            //sqlite3_finalize(stmt_replace_tick);
            ///TRADING_DB_TICK_DB_PRINT << data_base_name << " commit" << std::endl;
            if (!transaction.commit()) return false;
            ///TRADING_DB_TICK_DB_PRINT << data_base_name << " end commit" << std::endl;
            return true;
        }

        bool replace(std::deque<uint64_t> &buffer, utility::SqliteTransaction &transaction, utility::SqliteStmt &stmt) {
            if (buffer.empty()) return true;
            ///TRADING_DB_TICK_DB_PRINT << data_base_name << " replace " << buffer.size() << std::endl;
            if (!transaction.begin_transaction()) return false;
            sqlite3_reset(stmt.get());
            for (size_t i = 0; i < buffer.size(); ++i) {
                if (sqlite3_bind_int64(stmt.get(), 1, static_cast<sqlite3_int64>(buffer[i])) != SQLITE_OK) {
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
            }
            if (!transaction.commit()) return false;
            return true;
        }

        bool replace(Note &item, utility::SqliteTransaction &transaction, utility::SqliteStmt &stmt) {
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

        bool open_db(sqlite3 *&sqlite_db_ptr, const std::string &db_name, const bool readonly = false) {
            // открываем и возможно еще создаем таблицу
            int flags = readonly ?
                (SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX | SQLITE_OPEN_WAL) :
                (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX | SQLITE_OPEN_WAL);
            if(sqlite3_open_v2(db_name.c_str(), &sqlite_db_ptr, flags, nullptr) != SQLITE_OK) {
                TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: " << sqlite3_errmsg(sqlite_db_ptr) << ", db name " << db_name << std::endl;
                sqlite3_close_v2(sqlite_db_ptr);
                sqlite_db_ptr = nullptr;
                return false;
            } else {
                sqlite3_busy_timeout(sqlite_db_ptr, busy_timeout);
                // создаем таблицу в базе данных, если она еще не создана
                const char *create_ticks_table_sql =
                    "CREATE TABLE IF NOT EXISTS 'Ticks' ("
                    "timestamp          INTEGER PRIMARY KEY NOT NULL, "
                    "server_timestamp   INTEGER             NOT NULL, "
                    "bid                REAL                NOT NULL, "
                    "ask                REAL                NOT NULL)";
                const char *create_end_tick_table_sql =
                    "CREATE TABLE   IF NOT EXISTS 'EndTickStamp' ("
                    "timestamp      INTEGER     PRIMARY KEY NOT NULL)";
                const char *create_note_table_sql =
                    "CREATE TABLE IF NOT EXISTS 'Note' ("
                    "key                TEXT    PRIMARY KEY NOT NULL,"
                    "value              TEXT                NOT NULL)";

                if (!utility::prepare(sqlite_db_ptr, create_ticks_table_sql)) return false;
                if (!utility::prepare(sqlite_db_ptr, create_end_tick_table_sql)) return false;
                if (!utility::prepare(sqlite_db_ptr, create_note_table_sql)) return false;
                if (!exec_db(sqlite_db_ptr, "PRAGMA synchronous = NORMAL")) return false;
                if (!exec_db(sqlite_db_ptr, "PRAGMA wal_checkpoint(FULL)")) return false;
                if (!exec_db(sqlite_db_ptr, "PRAGMA cache_size = " + std::to_string(sqlite_cache_size))) return false;
                if (!exec_db(sqlite_db_ptr, "PRAGMA auto_vacuum = NONE")) return false;
                if (!exec_db(sqlite_db_ptr, "PRAGMA temp_store = MEMORY")) return false;
                if (!exec_db(sqlite_db_ptr, "PRAGMA wal_autocheckpoint = 10000")) return false;
            }
            return true;
        }

        bool init_db(const std::string &db_name, const bool readonly = false) {
            // открываем и возможно еще создаем таблицу
            /*
            int flags = readonly ?
                (SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX | SQLITE_OPEN_WAL) :
                (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX | SQLITE_OPEN_WAL);
            if(sqlite3_open_v2(db_name.c_str(), &this->sqlite_db, flags, nullptr) != SQLITE_OK) {
                TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: " << sqlite3_errmsg(sqlite_db) << ", db name " << db_name << std::endl;
                sqlite3_close_v2(this->sqlite_db);
                this->sqlite_db = nullptr;
                is_error = true;
                return false;
                */
            if (!open_db(this->sqlite_db, db_name, readonly)) {
                sqlite3_close_v2(this->sqlite_db);
                this->sqlite_db = nullptr;
                is_error = true;
                return false;
            } else {
                /*
                sqlite3_busy_timeout(this->sqlite_db, busy_timeout);
                // создаем таблицу в базе данных, если она еще не создана
                const char *create_ticks_table_sql =
                    "CREATE TABLE IF NOT EXISTS 'Ticks' ("
                    "timestamp          INTEGER PRIMARY KEY NOT NULL, "
                    "server_timestamp   INTEGER             NOT NULL, "
                    "bid                REAL                NOT NULL, "
                    "ask                REAL                NOT NULL)";
                const char *create_end_tick_table_sql =
                    "CREATE TABLE   IF NOT EXISTS 'EndTickStamp' ("
                    "timestamp      INTEGER     PRIMARY KEY NOT NULL)";
                const char *create_note_table_sql =
                    "CREATE TABLE IF NOT EXISTS 'Note' ("
                    "key                TEXT    PRIMARY KEY NOT NULL,"
                    "value              TEXT                NOT NULL)";

                if (!utility::prepare(this->sqlite_db, create_ticks_table_sql)) return false;
                if (!utility::prepare(this->sqlite_db, create_end_tick_table_sql)) return false;
                if (!utility::prepare(this->sqlite_db, create_note_table_sql)) return false;
                if (!exec_db("PRAGMA synchronous = NORMAL")) return false;
                if (!exec_db("PRAGMA wal_checkpoint(FULL)")) return false;
                if (!exec_db("PRAGMA cache_size = " + std::to_string(sqlite_cache_size))) return false;
                if (!exec_db("PRAGMA auto_vacuum = NONE")) return false;
                if (!exec_db("PRAGMA temp_store = MEMORY")) return false;
                if (!exec_db("PRAGMA wal_autocheckpoint = 10000")) return false;
                */

                sqlite_transaction.init(this->sqlite_db);
                stmt_get_first_ticks_lower.init(this->sqlite_db, "SELECT * FROM 'Ticks' WHERE timestamp <= :x ORDER BY timestamp DESC LIMIT :y");
                stmt_get_first_ticks_upper.init(this->sqlite_db, "SELECT * FROM 'Ticks' WHERE timestamp >= :x ORDER BY timestamp ASC LIMIT :y");
                stmt_get_first_ticks_lower_2.init(this->sqlite_db, "SELECT * FROM 'Ticks' WHERE server_timestamp <= :x ORDER BY server_timestamp DESC LIMIT :y");
                stmt_get_first_ticks_upper_2.init(this->sqlite_db, "SELECT * FROM 'Ticks' WHERE server_timestamp >= :x ORDER BY server_timestamp ASC LIMIT :y");
                stmt_get_first_end_tick_lower.init(this->sqlite_db, "SELECT * FROM 'EndTickStamp' WHERE timestamp <= :x ORDER BY timestamp DESC LIMIT :y");
                stmt_get_first_end_tick_upper.init(this->sqlite_db, "SELECT * FROM 'EndTickStamp' WHERE timestamp >= :x ORDER BY timestamp ASC LIMIT :y");

                stmt_replace_note.init(this->sqlite_db, "INSERT OR REPLACE INTO 'Note' (key, value) VALUES (?, ?)");
                stmt_get_note.init(this->sqlite_db, "SELECT * FROM 'Note' WHERE key == :x");
                stmt_get_min.init(this->sqlite_db, "SELECT min(timestamp) FROM 'Ticks'");
                stmt_get_max.init(this->sqlite_db, "SELECT max(timestamp) FROM 'Ticks'");
            }
            return true;
        }

        const size_t wait_delay = 10;
        const size_t wait_process_delay = 500;

        utility::AsyncTasks async_tasks;

        std::deque<Tick> write_tick_buffer;
        std::deque<uint64_t> write_end_tick_stamp_buffer;
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

        /** \brief Найти тик по метке времени
         */
        template<class CONTAINER_TYPE>
		inline typename CONTAINER_TYPE::value_type find_for_timestamp(
                const CONTAINER_TYPE &buffer,
                const uint64_t timestamp_ms) noexcept {
            typedef typename CONTAINER_TYPE::value_type type;
            if (buffer.empty()) return type();
            auto it = std::lower_bound(
                buffer.begin(),
                buffer.end(),
                timestamp_ms,
                [](const type & l, const uint64_t timestamp_ms) {
                    return  l.timestamp < timestamp_ms;
                });
            if (it == buffer.end()) {
                auto prev_it = std::prev(it, 1);
                if(prev_it->timestamp <= timestamp_ms) return *prev_it;
            } else
            if (it == buffer.begin()) {
                if(it->timestamp == timestamp_ms) return *it;
            } else {
                if(it->timestamp == timestamp_ms) {
                    return *it;
                } else {
                    auto prev_it = std::prev(it, 1);
                    return *prev_it;
                }
            }
            return type();
		}


		/** \brief Отсортировать контейнер с метками времени
         * \param buffer Неотсортированный контейнер с метками времени
         */
        template<class CONTAINER_TYPE>
        constexpr inline void sort_for_timestamp(CONTAINER_TYPE &buffer) noexcept {
            typedef typename CONTAINER_TYPE::value_type type;
            if (buffer.size() <= 1) return;
            if (!std::is_sorted(
                buffer.begin(),
                buffer.end(),
                [](const type & a, const type & b) {
                    return a.timestamp < b.timestamp;
                })) {
                std::sort(buffer.begin(), buffer.end(),
                [](const type & a, const type & b) {
                    return a.timestamp < b.timestamp;
                });
            }
        }

        /** \brief Инициализация объектов класса
         */
		inline void init_other() noexcept {
            async_tasks.create_task([&]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));

                // создаем подготовленные SQL команды
                utility::SqliteTransaction sqlite_transaction;
                utility::SqliteStmt stmt_replace_tick;
                utility::SqliteStmt stmt_replace_end_tick_stamp;

                sqlite_transaction.init(this->sqlite_db);
                {
                    const char *query =
                        "INSERT OR REPLACE INTO 'Ticks' (timestamp, server_timestamp, bid, ask) "
                        "VALUES (?, ?, ?, ?)";
                    stmt_replace_tick.init(this->sqlite_db, query);
                }
                {
                    const char *query =
                        "INSERT OR REPLACE INTO 'EndTickStamp' (timestamp) "
                        "VALUES (?)";
                    stmt_replace_end_tick_stamp.init(this->sqlite_db, query);
                }

                uint64_t last_tick_timestamp = 0; // метка времени последнего тика
                while (true) {
                    // данные для записи
                    std::deque<Tick> tick_buffer;
                    std::deque<uint64_t> end_tick_stamp_buffer;
                    {
                        // запоминаем данные для записи
                        std::lock_guard<std::mutex> lock(write_buffer_mutex);
                        if (xtime::get_timestamp() > last_reset_timestamp && is_recording) {
                            is_stop_write = true;
                        }
                        if (write_tick_buffer.size() > write_buffer_size_trigger || is_stop_write) {
                            if (!write_tick_buffer.empty()) {
                                const size_t buffer_size = std::min(write_tick_buffer.size(), max_buffer_size_commit);
                                tick_buffer.resize(buffer_size);
                                tick_buffer.assign(
                                    write_tick_buffer.begin(),
                                    write_tick_buffer.begin() + buffer_size);
                                last_tick_timestamp = write_tick_buffer[buffer_size - 1].timestamp;
                            }
                            if (is_stop_write) {
                                if (last_tick_timestamp != 0) write_end_tick_stamp_buffer.push_back(last_tick_timestamp);
                                is_recording = false;
                                is_stop_write = false;
                            }
                            if (!write_end_tick_stamp_buffer.empty()) {
                                const size_t buffer_size = std::min(write_end_tick_stamp_buffer.size(), max_buffer_size_commit);
                                end_tick_stamp_buffer.resize(buffer_size);
                                end_tick_stamp_buffer.assign(
                                    write_end_tick_stamp_buffer.begin(),
                                    write_end_tick_stamp_buffer.begin() + buffer_size);
                            }
                        }
                    }

                    // записываем данные в БД
                    if (!tick_buffer.empty() || !end_tick_stamp_buffer.empty()) {
                        if (!replace(tick_buffer, sqlite_transaction, stmt_replace_tick)) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(wait_delay));
                            continue;
                        }
                        if (!replace(end_tick_stamp_buffer, sqlite_transaction, stmt_replace_end_tick_stamp)) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(wait_delay));
                            continue;
                        }
                        std::lock_guard<std::mutex> lock(write_buffer_mutex);
                        /* очищаем буфер с котировками */
                        if (!write_tick_buffer.empty() && !tick_buffer.empty()) {
                            write_tick_buffer.erase(
                                write_tick_buffer.begin(),
                                write_tick_buffer.begin() + tick_buffer.size());
                        }
                        if (!write_end_tick_stamp_buffer.empty() && !end_tick_stamp_buffer.empty()) {
                            write_end_tick_stamp_buffer.erase(
                                write_end_tick_stamp_buffer.begin(),
                                write_end_tick_stamp_buffer.begin() + end_tick_stamp_buffer.size());
                        }
                    }

                    if (is_shutdown) break;
                    std::this_thread::sleep_for(std::chrono::milliseconds(wait_delay));
                }
                is_writing = false;
                is_stop = true;
            });
		}

		template<class T>
		inline T get_tick_array_from_db(utility::SqliteStmt &stmt, const uint64_t timestamp_ms, const size_t length) {
            T buffer;
            int err = 0;
            while (true) {
                if ((err = sqlite3_reset(stmt.get())) != SQLITE_OK) {
                    TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: sqlite3_reset return code " << err << std::endl;
                    return T();
                }
                if ((err = sqlite3_bind_int64(stmt.get(), 1, static_cast<sqlite3_int64>(timestamp_ms))) != SQLITE_OK) {
                    TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: sqlite3_bind_int64 return code " << err << std::endl;
                    return T();
                }
                if ((err = sqlite3_bind_int64(stmt.get(), 2, static_cast<sqlite3_int64>(length))) != SQLITE_OK) {
                    TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: sqlite3_bind_int64 return code " << err << std::endl;
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
                for (size_t i = 0; i < length; ++i) {
                    Tick tick;
                    tick.timestamp = sqlite3_column_int64(stmt.get(),0);
                    tick.server_timestamp = sqlite3_column_int64(stmt.get(),1);
                    tick.bid = sqlite3_column_double(stmt.get(),2);
                    tick.ask = sqlite3_column_double(stmt.get(),3);
                    buffer.push_back(tick);
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
                        TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: sqlite3_step return SQLITE_BUSY" << std::endl;
                        //return std::deque<Tick>();
                        break;
                    }
                }
                if(err == SQLITE_BUSY) {
                    sqlite3_reset(stmt.get());
                    sqlite3_clear_bindings(stmt.get());
                    buffer.clear();
                    TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: sqlite3_step return SQLITE_BUSY" << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }
                break;
            }
            return std::move(buffer);
		}

		inline Note get_note_from_db(utility::SqliteStmt &stmt, const std::string &key) {
            Note note;
            int err = 0;
            while (true) {
                if ((err = sqlite3_reset(stmt.get())) != SQLITE_OK) {
                    TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: sqlite3_reset return code " << err << std::endl;
                    return Note();
                }
                if (sqlite3_bind_text(stmt.get(), 1, key.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
                    TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: sqlite3_reset return code " << err << std::endl;
                    return Note();
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
                    return Note();
                }

                note.key = (const char *)sqlite3_column_text(stmt.get(),0);
                note.value = (const char *)sqlite3_column_text(stmt.get(),1);
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
                    note = Note();
                    TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: sqlite3_step return SQLITE_BUSY" << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }
                break;
            }
            return std::move(note);
		}

		bool backup_form_db(const std::string &path, sqlite3 *source_connection) {
            sqlite3 *dest_connection = nullptr;

            int flags = (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX);
            if (sqlite3_open_v2(path.c_str(), &dest_connection, flags, nullptr) != SQLITE_OK) {
            //if (sqlite3_open(path.c_str(), &dest_connection) != SQLITE_OK) {
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

	public:

        /** \brief Конструктор хранилища тиков
         * \param path Путь к файлу
         */
		TickDatabase(const std::string &path, const bool readonly = false) :
            database_name(path) {
            if (init_db(path, readonly)) {
                init_other();
            }
		}

		~TickDatabase() {
            is_block_write = true;
            // ставим флаг остановки записи
            {
                std::lock_guard<std::mutex> lock(write_buffer_mutex);
                is_stop_write = true;
            }
            while (is_stop_write) {
                std::this_thread::sleep_for(std::chrono::milliseconds(wait_delay));
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

        /** \brief Записать новый тик
         * \param new_tick  Новый тик
         * \return Вернет true в случае успешной записи
         */
        bool write(const Tick& new_tick) noexcept {
            // флаг блокировки 'is_block_write' записи имеет высокий приоритет
            if (is_block_write) return false;
            reset_counter_autostop_recording();
            std::lock_guard<std::mutex> lock(write_buffer_mutex);
            // проверяем остановку записи
            if (is_stop_write) return false;
            // ставим флаг записи
            is_recording = true;

            if (write_tick_buffer.empty()) {
                write_tick_buffer.push_back(new_tick);
            } else
            if (new_tick.timestamp > write_tick_buffer.back().timestamp) {
                write_tick_buffer.push_back(new_tick);
            } else
            if (new_tick.timestamp == write_tick_buffer.back().timestamp) {
                write_tick_buffer.back() = new_tick;
            } else {
                write_end_tick_stamp_buffer.push_back(write_tick_buffer.back().timestamp);
                write_tick_buffer.push_back(new_tick);
            }
			return true;
		}

		/** \brief Записать новый тик
         * \param new_tick  Новый тик
         * \return Вернет true в случае успешной записи
         */
		bool write(
                const uint64_t timestamp,
                const uint64_t server_timestamp,
                const double bid,
                const double ask) noexcept {
            return write(Tick(timestamp, server_timestamp, bid, ask));
		}

        /** \brief Остановить запись тиков
         */
		bool stop_write() noexcept {
            std::lock_guard<std::mutex> lock(write_buffer_mutex);
            if (!is_recording) return false;
            if (is_stop_write) return false;
            is_stop_write = true;
            return true;
		}

        /** \brief Установить время автоматического завершения записи тиков
         * \param standby_time Время автоматического завершения записи тиков
         */
		inline void set_timeout_autostop_recording(const uint32_t standby_time) noexcept {
            standby_time_end_ticks = standby_time;
		}

        /** \brief Сбросить счетчик автоматического завершения записи тиков
         */
		inline void reset_counter_autostop_recording() noexcept {
            const uint64_t temp = standby_time_end_ticks + xtime::get_timestamp();
            last_reset_timestamp = temp;
		}

        /** \brief Ожидание завершения записи тиков
         */
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

        /** \brief Создать бэкап
         * \param path  Путь к бэкапу базы данных
         * \return Вернет true, если бэкап начался
         */
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

    private:
		// переменные для работы с get_first_upper
		std::deque<Tick> first_upper_buffer_2;
        xtime::period_t first_upper_period_2;
		std::deque<Tick> first_upper_buffer_1;
		xtime::period_t first_upper_period_1;
		size_t first_upper_buffer_size = 10000;
		//size_t first_upper_buffer_index = 0;

        template<class T>
		inline Tick get_first_upper(T &buffer, xtime::period_t &period, const uint64_t timestamp_ms, const bool use_server_time = false) {
            if (buffer.empty() ||
                timestamp_ms < period.start ||
                timestamp_ms > period.stop) {

                period.start = timestamp_ms;
                buffer = get_first_upper_array<T>(timestamp_ms, first_upper_buffer_size, true);
                if (buffer.empty()) period.stop = timestamp_ms;
                else period.stop = use_server_time ? buffer.back().server_timestamp : buffer.back().timestamp;
            }

            if (!buffer.empty()) {
                if (use_server_time) {
                    auto it = std::lower_bound(
                        buffer.begin(),
                        buffer.end(),
                        timestamp_ms,
                        [](const Tick &lhs, const uint64_t &timestamp) {
                        return lhs.server_timestamp < timestamp;
                    });
                    if (it == buffer.end()) return Tick();
                    return *it;
                } else {
                    auto it = std::lower_bound(
                        buffer.begin(),
                        buffer.end(),
                        timestamp_ms,
                        [](const Tick &lhs, const uint64_t &timestamp) {
                        return lhs.timestamp < timestamp;
                    });
                    if (it == buffer.end()) return Tick();
                    return *it;
                }
            }
            return Tick();
		}

		public:

        /** \brief Получить первый ближайший тик выше указанной метки времени
         * \param timestamp_ms      Метка времени
         * \param use_server_time   Тип времени, если true, используется время сервера
         * \return Тик, время которого равно или больше указанной метки времени
         */
		inline Tick get_first_upper(const uint64_t timestamp_ms, const bool use_server_time = false) {
            if (use_server_time) {
                return get_first_upper<std::deque<Tick>>(first_upper_buffer_2, first_upper_period_2, timestamp_ms, true);
            } else {
                return get_first_upper<std::deque<Tick>>(first_upper_buffer_1, first_upper_period_1, timestamp_ms, false);
            }
            return Tick();
		}

		/** \brief Получить массив ближайших тиков выше указанной метки времени
         * \param timestamp_ms      Метка времени
         * \param length            Длина массива
         * \param use_server_time   Тип времени, если true, используется время сервера
         * \return Массив тиков, время которых равно или больше указанной метки времени
         */
        template<class T>
		inline T get_first_upper_array(const uint64_t timestamp_ms, const size_t length, const bool use_server_time = false) {
            std::lock_guard<std::mutex> lock(stmt_mutex);
            T temp;
            if (use_server_time) {
                temp = get_tick_array_from_db<T>(stmt_get_first_ticks_upper_2, timestamp_ms, length);
            } else {
                temp = get_tick_array_from_db<T>(stmt_get_first_ticks_upper, timestamp_ms, length);
            }
            if (temp.size() < length) temp.resize(length, Tick());
            return std::move(temp);
		}

        /** \brief Получить первый тик, который меньше или равен указанной метки времени
         * \param timestamp_ms      Метка времени, от которой ищем тик
         * \param use_server_time   Использовать время сервера
         * \return Вернет тик или пустой тик, если тик не найден
         */
		inline Tick get_first_lower(const uint64_t timestamp_ms, const bool use_server_time = false) {
            std::lock_guard<std::mutex> lock(stmt_mutex);
            std::deque<Tick> temp;
            if (use_server_time) {
                temp = get_tick_array_from_db<std::deque<Tick>>(stmt_get_first_ticks_lower_2, timestamp_ms, 1);
                if (temp.empty()) return Tick();
            } else {
                temp = get_tick_array_from_db<std::deque<Tick>>(stmt_get_first_ticks_lower, timestamp_ms, 1);
                if (temp.empty()) return Tick();
            }
            return temp[0];
		}

		inline Tick get_first_lower_multithreaded(const uint64_t timestamp_ms, const bool use_server_time = false) {
            utility::SqliteStmt stmt;
            if (use_server_time) {
                const char *query = "SELECT * FROM 'Ticks' WHERE server_timestamp <= :x ORDER BY server_timestamp DESC LIMIT :y";
                stmt.init(this->sqlite_db, query);
            } else {
                const char *query = "SELECT * FROM 'Ticks' WHERE timestamp <= :x ORDER BY timestamp DESC LIMIT :y";
                stmt.init(this->sqlite_db, query);
            }
            std::deque<Tick> temp = get_tick_array_from_db<std::deque<Tick>>(stmt, timestamp_ms, 1);
            if (temp.empty()) return Tick();
            return temp[0];
		}

        /** \brief Получить период данных, имеющихся в БД
         * \return Вернет структуру, содержащую даты начала и конца данных тиков
         */
		xtime::period_t get_data_period() {
            std::lock_guard<std::mutex> lock(stmt_mutex);
            xtime::period_t p;
            p.start = get_min_max_from_db(stmt_get_min);
            p.stop = get_min_max_from_db(stmt_get_max);
            p.start /= 1000;
            p.stop /= 1000;
            return p;
		}

		/** \brief Бэктестинг
         *
         * Данный метод вызывает функцию обратного вызова для каждого тика за указанный период
         * \param data_period   Период бэкстестинга, в секундах
         * \param callback      Функция обратного вызова
         */
        void backtesting_witch_stop(
                xtime::period_t data_period,
                const std::function<void(const Tick &tick, const bool is_stop)> &callback) {
            data_period.start *= xtime::MILLISECONDS_IN_SECOND;
            data_period.stop *= xtime::MILLISECONDS_IN_SECOND;
            xtime::timestamp_t last_timestamp = data_period.start;
            xtime::timestamp_t end_timestamp = 0;
            for(xtime::timestamp_t t = data_period.start; t <= data_period.stop; ++t) {
                Tick first_tick = get_first_upper(t);
                if (!first_tick.empty()) {
                    t = last_timestamp = first_tick.timestamp;
                    if (last_timestamp > end_timestamp || end_timestamp == 0) {
                        Tick first_end = get_first_upper(t);
                        if (!first_end.empty()) {
                            end_timestamp = first_end.timestamp;
                        } else {
                            //?
                        }
                    }
                    callback(first_tick, (last_timestamp == end_timestamp));
                }
            } // for t
		}

		/** \brief Бэктестинг
         *
         * Данный метод обрабатывает массив баз данных и вызывает функцию обратного вызова для каждого тика за указанный период
         * \param array_db      Массив баз данных
         * \param data_period   Период бэкстестинга, в секундах
         * \param callback      Функция обратного вызова
         */
        template<class T>
		static void backtesting(
                T &array_db,
                xtime::period_t data_period,
                const std::function<void(
                    const size_t index_db,
                    const Tick &tick)> &callback) {
            data_period.start *= xtime::MILLISECONDS_IN_SECOND;
            data_period.stop *= xtime::MILLISECONDS_IN_SECOND;

            const size_t buffer_size = 20000;
            const size_t db_count = array_db.size();

            std::deque<uint64_t> db_time(db_count, data_period.start); // время следующего тика
            std::deque<std::deque<Tick>> buffer(db_count); // массив тиков для всех БД
            std::deque<uint64_t> buffer_index(db_count); // индексы тиков в массивах

            // начальная инициализация массивов
            for (size_t index = 0; index < db_count; ++index) {
                buffer[index] = array_db[index]->template get_first_upper_array<std::deque<Tick>>(db_time[index], buffer_size, false);
                buffer_index[index] = 1;
                if (buffer[index][0].empty()) {
                    db_time[index] = data_period.stop;
                } else {
                    db_time[index] = buffer[index][0].timestamp + 1;
                }
            } // for

            while (true) {
                // ищем минимальный по времени тик
                uint64_t min_next_timestamp = std::numeric_limits<uint64_t>::max();
                size_t index_min = 0;
                for (size_t index = 0; index < db_count; ++index) {
                    const Tick &tick = buffer[index][buffer_index[index]];
                    if (!tick.empty() && min_next_timestamp >= tick.timestamp) {
                        min_next_timestamp = tick.timestamp;
                        index_min = index;
                    }
                }
                // тиков больше нет, выходим
                if (min_next_timestamp == std::numeric_limits<uint64_t>::max()) break;
                if (min_next_timestamp >= data_period.stop) break;

                // вызываем функцию обратного вызова
                db_time[index_min] = min_next_timestamp + 1;
                callback(index_min, buffer[index_min][buffer_index[index_min]]);
                ++buffer_index[index_min];

                if (buffer_index[index_min] == buffer[index_min].size()) {
                    buffer[index_min] = array_db[index_min]->template get_first_upper_array<std::deque<Tick>>(db_time[index_min], buffer_size, false);
                    buffer_index[index_min] = 0;
                }
            };
		}

        enum TypesPrice {
            USE_BID = 0,        /**< Использовать цену bid */
            USE_ASK = 1,        /**< Использовать цену ask */
            USE_AVERAGE = 2,    /**< Использовать среднюю цену */
		};

		enum TypesDirections {
            DIR_ERROR = -2,     /**< Ошибка получения направления */
            DIR_UNDEFINED = 0,  /**< Неопределенное направление */
            DIR_UP = 1,         /**< Цена двигалась верх */
            DIR_DOWN = -1       /**< Цена двигалась вниз */
		};

        /** \brief Получить направление цены (имитация бинарного опциона)
         * \param type          Тип цены
         * \param timestamp_ms  Время, от которого начинаем проверку направления
         * \param expiration_ms Экспирация
         * \param delay_ms      Задержка
         * \param period_ms     Период дискретизации. Указать 0, если замер происходит в любую миллисекунду
         * \param between_ticks_ms  Максимальное время между тиками. Если время меньше - ошибка.
         * \return Вернет направление цены или ошибку
         */
		inline TypesDirections get_direction(
                const TypesPrice type,
                const uint64_t timestamp_ms,
                const uint64_t expiration_ms,
                const uint64_t delay_ms,
                const uint64_t period_ms = 0,
                const uint64_t between_ticks_ms = 20000) noexcept {
            const uint64_t t1_delay = timestamp_ms + delay_ms;
            const uint64_t t1 = period_ms == 0 ? t1_delay : (t1_delay - (t1_delay % period_ms) + period_ms);
            Tick tick_1 = get_first_lower(t1, false);
            if (tick_1.empty()) return TypesDirections::DIR_ERROR;
            if ((t1 - tick_1.timestamp) > between_ticks_ms) return TypesDirections::DIR_ERROR;
            const uint64_t t2 = t1 + expiration_ms;
            Tick tick_2 = get_first_lower(t2, false);
            if (tick_2.empty()) return TypesDirections::DIR_ERROR;
            if ((t2 - tick_2.timestamp) > between_ticks_ms) return TypesDirections::DIR_ERROR;
            const double open = type == TypesPrice::USE_BID ? tick_1.bid : (type == TypesPrice::USE_ASK ? tick_1.ask : ((tick_1.bid + tick_1.ask)/2.0d));
            const double close = type == TypesPrice::USE_BID ? tick_2.bid : (type == TypesPrice::USE_ASK ? tick_2.ask : ((tick_2.bid + tick_2.ask)/2.0d));
            /*
            std::cout << "o " << open << " c " << close << " "
                << xtime::get_str_time_ms((double)timestamp_ms / (double)xtime::MILLISECONDS_IN_SECOND)
                << " "
                << xtime::get_str_time_ms((double)tick_1.timestamp / (double)xtime::MILLISECONDS_IN_SECOND)
                << " "
                << xtime::get_str_time_ms((double)tick_2.timestamp / (double)xtime::MILLISECONDS_IN_SECOND)
                << std::endl;
                */
            if (close > open) return TypesDirections::DIR_UP;
            if (close < open) return TypesDirections::DIR_DOWN;
            return TypesDirections::DIR_UNDEFINED;
		}

        /** \brief Установить заметку
         * \param key   Ключ заметки
         * \param value Значение заметки
         * \return Вернет true в случае успешного завершения
         */
		bool set_note(const std::string& key, const std::string& value) noexcept {
		    Note kv{key, value};
            while (true) {
                std::lock_guard<std::mutex> lock(stmt_mutex);
                if (replace(kv, sqlite_transaction, stmt_replace_note)) return true;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            return false;
		}

        /** \brief Получить заметку по ключу
         * \param key   Ключ заметки
         * \return Значение заметки
         */
		std::string get_note(const std::string& key) noexcept {
			std::lock_guard<std::mutex> lock(stmt_mutex);
            return get_note_from_db(stmt_get_note, key).value;
		}

		bool set_symbol(const std::string& symbol) noexcept {
            return set_note("symbol", symbol);
		}

		std::string get_symbol() noexcept {
            return get_note("symbol");
		}

        bool set_digits(const uint32_t digits) noexcept {
            return set_note("digits", std::to_string(digits));
		}

		uint32_t get_digits() noexcept {
            return std::stoi(get_note("digits"));
		}

        bool set_server_name(const std::string &server_name) noexcept {
            return set_note("server_name", server_name);
		}

		std::string get_server_name() noexcept {
            return get_note("server_name");
		}

        bool set_hostname(const std::string& hostname) noexcept {
            return set_note("hostname", hostname);
		}

		std::string get_hostname() noexcept {
            return get_note("hostname");
		}

		bool set_comment(const std::string& comment) noexcept {
            return set_note("comment", comment);
		}

		std::string get_comment() noexcept {
            return get_note("comment");
		}
	};
};

#endif /* TRADING_TICK_DB_HPP */
