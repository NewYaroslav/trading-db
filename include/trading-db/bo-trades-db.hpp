#pragma once
#ifndef TRADING_DB_BO_TRADES_DB_HPP_INCLUDED
#define TRADING_DB_BO_TRADES_DB_HPP_INCLUDED

#if SQLITE_THREADSAFE != 1
#error "The project must be built for sqlite multithreading! Set the SQLITE_THREADSAFE=1"
#endif

#include "config.hpp"
#include "parts/bo-trades-db/common.hpp"
#include "parts/bo-trades-db/stats.hpp"
#include "parts/bo-trades-db/meta-stats.hpp"

#include "utils/sqlite-func.hpp"
#include "utils/async-tasks.hpp"
#include "utils/safe-queue.hpp"
#include "utils/print.hpp"
#include "utils/files.hpp"

#include <ztime.hpp>

#include "parts/ztime_timer.hpp"

#include <mutex>
#include <atomic>
#include <future>
#include <vector>
#include <map>
#include <set>

namespace trading_db {

    /** \brief Класс базы данных ставок БО
     */
    class BoTradesDB {
    public:

        /** \brief Класс конфигурации БД
         */
        class Config {
        public:
            const std::string title         = "trading_db::BoTradesDB ";
            const std::string db_version    = "1.0";    /**< Версия БД */
            const std::string key_db_version    = "version";
            const std::string key_update_date   = "update-date";
            const std::string key_bet_uid       = "bet-id";
            double idle_time        = 15;   /**< Время бездействия записи */
            double meta_data_time   = 1;    /**< Время обновления мета данных */
            size_t busy_timeout     = 0;    /**< Время ожидания БД */
            size_t threshold_bets   = 1000; /**< Порог срабатывания по количеству сделок */
            std::atomic<bool> read_only = ATOMIC_VAR_INIT(false);
            std::atomic<bool> use_log   = ATOMIC_VAR_INIT(false);
        };

        Config config;  /**< Конфигурация БД */

        using ContractType = bo_trades_db::ContractType;
        using BoType = bo_trades_db::BoType;
        using BoStatus = bo_trades_db::BoStatus;
        using BoResult = bo_trades_db::BoResult;

    private:
        std::string database_name;
        sqlite3 *sqlite_db = nullptr;
        // команды для транзакций
        utils::SqliteTransaction sqlite_transaction;
        // предкомпилированные команды
        utils::SqliteStmt stmt_replace_bet;
        utils::SqliteStmt stmt_replace_meta_data;
        utils::SqliteStmt stmt_get_meta_data;
        utils::SqliteStmt stmt_get_bet;
        utils::SqliteStmt stmt_get_all_bet;

        //utils::SafeQueue<BoResult> bet_safe_queue;
        std::deque<BoResult> bo_queue;
        std::mutex bo_queue_mutex;

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

        utils::AsyncTasks   async_tasks;
        ztime::Timer        timer_tasks;
        ztime::Timer        timer_meta_data;

        /** \brief Класс для хранения мета данных
         */
        class MetaData {
        public:
            std::string key;
            std::string value;

            MetaData() {};

            MetaData(const std::string &k, const std::string &v) : key(k), value(v) {};
        };

        std::atomic<uint64_t> last_update_date = ATOMIC_VAR_INIT(0);    /**< Последнее время обновления БД */

        inline void print_error(const std::string message, const int line) noexcept {
            if (config.use_log) {
                TRADING_DB_TICK_DB_PRINT
                    << config.title << "error in [file " << __FILE__
                    << ", line " << line
                    << "], message: " << message << std::endl;
            }
        }

        bool open_db(sqlite3 *&sqlite_db_ptr, const std::string &db_name, const bool readonly = false) {
            utils::create_directory(db_name, true);
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
                "uid            INTEGER NOT NULL,"
                "broker_id      INTEGER NOT NULL,"
                "open_date      INTEGER NOT NULL,"
                "close_date     INTEGER NOT NULL,"
                "open_price     REAL    NOT NULL,"
                "close_price    REAL    NOT NULL,"
                "amount         REAL    NOT NULL,"
                "profit         REAL    NOT NULL,"
                "payout         REAL    NOT NULL,"
                "winrate        REAL    NOT NULL,"
                "delay          INTEGER NOT NULL,"
                "ping           INTEGER NOT NULL,"
                "duration       INTEGER NOT NULL,"
                "step           INTEGER NOT NULL,"
                "demo           INTEGER NOT NULL,"
                "last           INTEGER NOT NULL,"
                "contract_type  INTEGER NOT NULL,"
                "status         INTEGER NOT NULL,"
                "type           INTEGER NOT NULL,"
                "symbol         TEXT    NOT NULL,"
                "broker         TEXT    NOT NULL,"
                "currency       TEXT    NOT NULL,"
                "signal         TEXT    NOT NULL,"
                "comment        TEXT    NOT NULL,"
                "user_data      TEXT    NOT NULL,"
                "PRIMARY KEY (open_date, uid))");

            const std::string create_meta_data_table_sql =
                    "CREATE TABLE IF NOT EXISTS 'meta-data' ("
                    "key                TEXT    PRIMARY KEY NOT NULL,"
                    "value              TEXT                NOT NULL)";

            const size_t delay_per_attempt = 250;
            const size_t attempts = 100;

            for (size_t i = 1; i <= attempts; ++i) {
                if (utils::prepare(sqlite_db_ptr, create_bets_table_sql)) break;
                if (i == attempts) return false;
                std::this_thread::sleep_for(std::chrono::milliseconds(delay_per_attempt));
            }
            for (size_t i = 1; i <= attempts; ++i) {
                if (utils::prepare(sqlite_db_ptr, create_meta_data_table_sql)) break;
                if (i == attempts) return false;
                std::this_thread::sleep_for(std::chrono::milliseconds(delay_per_attempt));
            }
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
                const std::deque<BoResult> &buffer,
                utils::SqliteTransaction &transaction,
                utils::SqliteStmt &stmt) {

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
                utils::SqliteTransaction &transaction,
                utils::SqliteStmt &stmt) noexcept {
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
        inline T get_bets_db(utils::SqliteStmt &stmt) noexcept {
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
                    bet_data.uid            = sqlite3_column_int64(stmt.get(), index++);
                    bet_data.broker_id      = sqlite3_column_int64(stmt.get(), index++);

                    bet_data.open_date      = sqlite3_column_double(stmt.get(), index++);
                    bet_data.close_date     = sqlite3_column_double(stmt.get(), index++);

                    bet_data.open_price     = sqlite3_column_double(stmt.get(), index++);
                    bet_data.close_price    = sqlite3_column_double(stmt.get(), index++);

                    bet_data.amount         = sqlite3_column_double(stmt.get(), index++);
                    bet_data.profit         = sqlite3_column_double(stmt.get(), index++);
                    bet_data.payout         = sqlite3_column_double(stmt.get(), index++);
                    bet_data.winrate        = sqlite3_column_double(stmt.get(), index++);

                    bet_data.delay          = sqlite3_column_int(stmt.get(), index++);
                    bet_data.ping           = sqlite3_column_int(stmt.get(), index++);

                    bet_data.duration       = static_cast<uint32_t>(sqlite3_column_int(stmt.get(), index++));
                    bet_data.step           = static_cast<uint32_t>(sqlite3_column_int(stmt.get(), index++));

                    bet_data.demo           = static_cast<bool>(sqlite3_column_int(stmt.get(), index++));
                    bet_data.last           = static_cast<bool>(sqlite3_column_int(stmt.get(), index++));

                    bet_data.contract_type  = static_cast<ContractType>(sqlite3_column_int(stmt.get(), index++));
                    bet_data.status         = static_cast<BoStatus>(sqlite3_column_int(stmt.get(), index++));
                    bet_data.type           = static_cast<BoType>(sqlite3_column_int(stmt.get(), index++));

                    bet_data.symbol     = (const char *)sqlite3_column_text(stmt.get(), index++);
                    bet_data.broker     = (const char *)sqlite3_column_text(stmt.get(), index++);
                    bet_data.currency   = (const char *)sqlite3_column_text(stmt.get(), index++);
                    bet_data.signal     = (const char *)sqlite3_column_text(stmt.get(), index++);
                    bet_data.comment    = (const char *)sqlite3_column_text(stmt.get(), index++);
                    bet_data.user_data  = (const char *)sqlite3_column_text(stmt.get(), index++);

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
                utils::SqliteStmt &stmt,
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

        void write_bo_result_buffer(std::deque<BoResult> &buffer) {
            // запишем данные
            replace_db(buffer, sqlite_transaction, stmt_replace_bet);
            // обновим мета-данные
            last_update_date = ztime::get_timestamp();
            MetaData meta_data_ut(config.key_update_date, std::to_string(last_update_date));

            std::unique_lock<std::mutex> lock(bet_uid_mutex);
            const int64_t update_bet_uid = bet_uid;
            lock.unlock();

            MetaData meta_data_lbu(config.key_bet_uid, std::to_string(update_bet_uid));
            // запишем мета-данные
            replace_db(meta_data_ut, sqlite_transaction, stmt_replace_meta_data);
            replace_db(meta_data_lbu, sqlite_transaction, stmt_replace_meta_data);
        }

        void write_bo_result() {
            if (!config.read_only) {
                std::unique_lock<std::mutex> lock(bo_queue_mutex);
                if (bo_queue.size() > config.threshold_bets ||
                    (!bo_queue.empty() &&
                        (is_flush || timer_tasks.elapsed() >= config.idle_time))) {
                    auto buffer = std::move(bo_queue);
                    lock.unlock();
                    write_bo_result_buffer(buffer);
                    timer_tasks.reset();
                    is_flush = false;
                } else {
                    lock.unlock();
                }
            } else {
                is_flush = false;
            }
        }

        static void async_task(void *user_data) {
            BoTradesDB *ptr = (BoTradesDB *)user_data;
            ptr->write_bo_result();
            //{ читаем время обновления
            if (ptr->timer_meta_data.elapsed() >= ptr->config.meta_data_time) {
                ptr->timer_meta_data.reset();
                const std::string value = ptr->get_meta_data_db(ptr->stmt_get_meta_data, ptr->config.key_update_date).value;
                try {
                    if (!value.empty()) {
                        ptr->last_update_date = std::stoll(value);
                    }
                } catch(...) {};
            }
            //}
        }

        // основная задача (запись данных в БД) для фонового процесса
        void main_task() {
            init_version();
            init_update_date();
            init_bet_uid();
            const int period_update_ms = 10;
            timer_tasks.create_event(
                period_update_ms,
                ztime::Timer::TimerMode::UNSTABLE_INTERVAL,
                async_task,
                this);
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

        BoTradesDB() {};

        /** \brief Конструктор хранилища тиков
         * \param path      Путь к файлу
         * \param readonly  Флаг 'только чтение'
         */
        BoTradesDB(const std::string &path, const bool readonly = false) {
            open(path, readonly);
        }

        ~BoTradesDB() {
            std::lock_guard<std::mutex> lock(method_mutex);
            timer_tasks.stop_event();
            // отправляем сигнал на очистку буфера
            is_flush = true;
            write_bo_result();
            // отправляем сигнал на выход из задачи
            is_shutdown = true;
            async_tasks.wait();
            // закрываем соединение
            if (sqlite_db) sqlite3_close_v2(sqlite_db);
        }

        /** \brief Открыть БД
         * \param path      Путь к файлу БД
         * \param readonly  Флаг 'только чтение'
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
        inline int64_t get_trade_uid() noexcept {
            std::lock_guard<std::mutex> lock(bet_uid_mutex);
            const int64_t temp = bet_uid;
            ++bet_uid;
            return temp;
        }

        /** \brief Добавить или обновить ставку в БД
         * Метод присвоит уникальный uid самостяотельно, если он не указан
         * \param bo_result Данные ставки
         * \return Вернет true, если ставка была добавлена в очередь назапись в БД
         */
        bool replace_trade(BoResult &bo_result) noexcept {
            if (config.read_only) return false;
            std::lock_guard<std::mutex> lock(method_mutex);
            if (!check_init_db()) return false;
            //{ проверяем данные на валидность
            if (bo_result.open_date <= 0) return false;
            if (bo_result.uid <= 0) bo_result.uid = get_trade_uid();
            //}
            std::lock_guard<std::mutex> lock_bo_queue(bo_queue_mutex);
            bo_queue.push_back(bo_result);
            return true;
        }

        /** \brief Очистить поток записи
         * Данный блокирующий метод принуждает БД провести запись данных немедленно
         */
        inline void flush() noexcept {
            if (config.read_only) return;
            std::lock_guard<std::mutex> lock(method_mutex);
            is_flush = true;
            while (is_flush && !is_shutdown) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        /** \brief Параметры запроса для выгрузки ставок из БД
         */
        class RequestConfig {
        public:
            int64_t start_date   = 0;                   /// Начальная дата запроса, в мс
            int64_t stop_date    = 0;                   /// Конечная дата запроса, в мс
            std::vector<std::string>    brokers;        /// Список брокеров. Если пусто, то не фильтруется
            std::vector<std::string>    no_brokers;     /// Список запрещенных брокеров. Если пусто, то не фильтруется
            std::vector<std::string>    symbols;        /// Список символов. Если пусто, то не фильтруется
            std::vector<std::string>    no_symbols;     /// Список запрещенных символов. Если пусто, то не фильтруется
            std::vector<std::string>    signals;        /// Список сигналов. Если пусто, то не фильтруется
            std::vector<std::string>    no_signals;     /// Список запрещенных сигналов. Если пусто, то не фильтруется
            std::vector<std::string>    currency;       /// Список валют счета. Если пусто, то не фильтруется
            std::vector<std::string>    no_currency;    /// Список запрещенных валют счета. Если пусто, то не фильтруется
            std::vector<uint32_t>       durations;      /// Список экспираций. Если пусто, то не фильтруется
            std::vector<uint32_t>       no_durations;   /// Список запрещенных экспираций. Если пусто, то не фильтруется
            std::vector<uint32_t>       hours;          /// Список торговых часов. Если пусто, то не фильтруется
            std::vector<uint32_t>       no_hours;       /// Список запрещенных торговых часов. Если пусто, то не фильтруется
            std::vector<uint32_t>       weekday;        /// Список торговых дней недели. Если пусто, то не фильтруется
            std::vector<uint32_t>       no_weekday;     /// Список запрещенных торговых дней недели. Если пусто, то не фильтруется
            uint32_t start_time  = 0;                   /// Начальное время торговли
            uint32_t stop_time   = 0;                   /// Конечное время торговли

            double min_amount    = 0;                   /// Минимальный размер ставки
            double max_amount    = 0;                   /// Максимальный размер ставки
            double min_payout    = 0;                   /// Минимальный процент выплат
            double max_payout    = 0;                   /// Максимальный процент выплат
            int64_t min_ping     = 0;                   /// Минимальный пинг запроса на открытие ставки в миллисекундах
            int64_t max_ping     = 0;                   /// Максимальный пинг запроса на открытие ставки в миллисекундах

            bool only_last       = false;               /// Использовать только последние сделки в цепочке (актуально для мартингейла)
            bool only_result     = false;               /// Использовать только сделки с результатом
            bool use_buy         = true;                /// Использовать сделки BUY
            bool use_sell        = true;                /// Использовать сделки SELL
            bool use_demo        = true;                /// Использовать сделки на DEMO
            bool use_real        = true;                /// Использовать сделки на REAL

        };

        /** \brief Получить данные из БД
         * \param request   Структура запроса, которая определяет фильтрацию данных
         * \return Вернет массив ставок, если есть. Если массив пустой, то данных удовлетворяющих запросу нет
         */
        template<class T>
        inline T get_trades(const RequestConfig &request) noexcept {
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
            utils::SqliteStmt stmt;
            stmt.init(sqlite_db, request_str);
            // получаем данные
            T buffer(get_bets_db<T>(stmt));

            // проводим оставшуюся фильтрацию
            size_t index = 0;
            while (index < buffer.size()) {
                // получаем метку времени
                const uint64_t timestamp = ztime::ms_to_sec(buffer[index].open_date);
                const uint32_t hour = ztime::get_hour_day(timestamp);
                const uint32_t weekday = ztime::get_weekday(timestamp);

                if (!check_list(request.weekday, false, weekday) ||     // фильтруем по дням недели
                    !check_list(request.no_weekday, true, weekday) ||   // фильтруем по дням недели
                    !check_list(request.hours, false, hour) ||          // фильтруем по часам
                    !check_list(request.no_hours, true, hour) ||        // фильтруем по часам
                    !check_list(request.durations, false, buffer[index].duration) ||    // фильтруем по часам
                    !check_list(request.no_durations, true, buffer[index].duration)) {  // фильтруем по часам
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
                    if (buffer[index].status != BoStatus::WIN &&
                        buffer[index].status != BoStatus::LOSS &&
                        buffer[index].status != BoStatus::STANDOFF) {
                        buffer.erase(buffer.begin() + index);
                        continue;
                    }
                }

                if (request.only_result) {
                    if (buffer[index].status != BoStatus::WIN &&
                        buffer[index].status != BoStatus::LOSS &&
                        buffer[index].status != BoStatus::STANDOFF) {
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

        /** \brief Очистить все данные
         */
        inline bool remove_all() {
            std::lock_guard<std::mutex> lock(method_mutex);
            if (!check_init_db()) return false;
            const size_t delay_per_attempt = 250;
            const size_t attempts = 100;
            for (size_t i = 1; i <= attempts; ++i) {
                if (utils::prepare(sqlite_db, "DELETE FROM 'bets-data-v1'")) break;
                if (i == attempts) return false;
                std::this_thread::sleep_for(std::chrono::milliseconds(delay_per_attempt));
            }
            return true;
        }

        /** \brief Удалить сделку
         * \param bo_result Заполненная структура сделки для удаления
         */
        inline bool remove_trade(const BoResult &bo_result) noexcept {
            std::lock_guard<std::mutex> lock(method_mutex);
            if (!check_init_db()) return false;
            const size_t delay_per_attempt = 250;
            const size_t attempts = 100;
            for (size_t i = 1; i <= attempts; ++i) {
                if (utils::prepare(sqlite_db,
                    "DELETE FROM 'bets-data-v1' WHERE open_date == " +
                    std::to_string(bo_result.open_date) +
                    " AND uid == " +
                    std::to_string(bo_result.uid))) break;
                if (i == attempts) return false;
                std::this_thread::sleep_for(std::chrono::milliseconds(delay_per_attempt));
            }
            return true;
        }

        /** \brief Удалить сделку по UID
         * \param uid   Уникальный номер сделки в БД
         */
        inline bool remove_trade(const int64_t uid) noexcept {
            std::lock_guard<std::mutex> lock(method_mutex);
            if (!check_init_db()) return false;
            const size_t delay_per_attempt = 250;
            const size_t attempts = 100;
            for (size_t i = 1; i <= attempts; ++i) {
                if (utils::prepare(sqlite_db,
                    "DELETE FROM 'bets-data-v1' WHERE uid == " +
                    std::to_string(uid))) break;
                if (i == attempts) return false;
                std::this_thread::sleep_for(std::chrono::milliseconds(delay_per_attempt));
            }
            return true;
        }

        /** \brief Удалить значения по ключам
         * \param keys  Массив ключей элементов списка
         */
        template<class T>
        inline bool remove_trades(const T &keys) noexcept {
            std::lock_guard<std::mutex> lock(method_mutex);
            if (!check_init_db()) return false;
            if (keys.empty()) return false;
            std::string message("DELETE FROM 'bets-data-v1' WHERE key IN (");
            for (auto it = keys.begin(); it != keys.end(); ++it) {
                if (it != keys.begin()) message += ",";
                message += *it;
            }
            message += ")";

            const size_t delay_per_attempt = 250;
            const size_t attempts = 100;
            for (size_t i = 1; i <= attempts; ++i) {
                if (utils::prepare(sqlite_db, message)) break;
                if (i == attempts) return false;
                std::this_thread::sleep_for(std::chrono::milliseconds(delay_per_attempt));
            }
            return true;
        }

        //----------------------------------------------------------------------
        using Stats = bo_trades_db::Stats;
        using MetaStats = bo_trades_db::MetaStats;
    };

}; // trading_db

#endif // TRADING_DB_BO_TRADES_DB_HPP_INCLUDED
