#pragma once
#ifndef TRADING_DB_BET_DATABASE_HPP_INCLUDED
#define TRADING_DB_BET_DATABASE_HPP_INCLUDED

#if SQLITE_THREADSAFE != 1
#error "The project must be built for sqlite multithreading! Set the SQLITE_THREADSAFE=1"
#endif

#include "config.hpp"
#include "utility/sqlite-func.hpp"
#include "utility/async-tasks.hpp"
#include "utility/print.hpp"
#include "utility/files.hpp"
#include <xtime.hpp>
#include <mutex>
#include <atomic>
#include <future>
#include <vector>

namespace trading_db {

    /** \brief Класс базы данных ставок БО
     */
	class BetDatabase {
    public:

        class Config {
        public:
            const std::string title = "trading_db::BetDatabase ";
            int busy_timeout    = 0;    /**< Время ожидания БД */
            double idle_time    = 15;   /**< Время бездействия записи */
            int threshold_bets = 100;   /**< Порог срабатывания по количеству сделок */
            std::atomic<bool> use_log = ATOMIC_VAR_INIT(false);
        };

        Config config;

		/// Направление ставки
        enum class ContractTypes {
            UNKNOWN_STATE = 0,
            BUY = 1,
            SELL = -1,
        };

		/// Тип ставки
        enum class BoTypes {
            SPRINT = 0,
            CLASSIC = 1,
        };

		/// Состояния сделки
        enum class BetStatus {
            UNKNOWN_STATE,          /**< Неопределенное состояние уже открытой сделки */
            OPENING_ERROR,          /**< Ошибка открытия */
            CHECK_ERROR,            /**< Ошибка проверки результата сделки */
            LOW_PAYMENT_ERROR,      /**< Низкий процент выплат */
            WAITING_COMPLETION,     /**< Ждем завершения сделки */
            WIN,                    /**< Победа */
            LOSS,                   /**< Убыток */
            STANDOFF,               /**< Ничья */
            UPDATE,                 /**< Обновление состояния ставки */
            INCORRECT_PARAMETERS,   /**< Некорректные параметры ставки */
            AUTHORIZATION_ERROR     /**< Ошибкаавторизации */
        };

        /** \brief Класс ставки БО
         */
        class BetData {
        public:
            int64_t uid         = 0;    /// ключ - уникальный ID сделки в БД
			int64_t broker_id 	= 0;	/// уникальный номер сделки, который присваивает брокер
            int64_t open_date 	= 0;	/// метка времени открытия сделки в миллисекундах
            int64_t close_date 	= 0;	/// метка времени закрытия сделки в миллисекундах
            double open_price 	= 0;	/// цена входа в сделку
            double close_price 	= 0;	/// цена выхода из сделки

            double amount 		= 0;	/// размер ставки
            double profit 		= 0;	/// размер выплаты
            double payout 		= 0;	/// процент выплат

			int64_t delay 		= 0; 	/// задержка на открытие ставки в миллисекундах
			int64_t ping 		= 0; 	/// пинг запроса на открытие ставки в миллисекундах

            uint32_t duration 	= 0; 	/// экспирация (длительность) бинарного опциона в секундах
			uint32_t step 		= 0;	/// шаг систем риск менеджмента
            bool demo 			= true;	/// флаг демо аккаунта
			bool last 			= true;	/// флаг последней сделки - для подсчета винрейта в системах риск-менджента типа мартингейла и т.п.

            ContractTypes contract_type = ContractTypes::UNKNOWN_STATE;	/// тип контракта, см.BetContractType
            BetStatus status 	= BetStatus::UNKNOWN_STATE;	/// состояние сделки, см.BetStatus
            BoTypes type 		= BoTypes::SPRINT;			/// тип бинарного опциона(SPRINT, CLASSIC и т.д.), см.BetType

            std::string symbol;	    /// имя символа(валютная пара, акции, индекс и пр., например EURUSD)
            std::string broker;		/// имя брокера
            std::string currency;	/// валюта ставки
            std::string signal;		/// имя сигнала, стратегии или индикатора, короче имя источника сигнала
            std::string comment;	/// комментарий
			std::string user_data;	/// данные пользователя

            BetData() {};
        };

    private:
        std::string database_name;
        sqlite3 *sqlite_db = nullptr;
        // команды для транзакций
        utility::SqliteTransaction sqlite_transaction;
        // предкомпилированные команды
        utility::SqliteStmt stmt_replace_bet;
        utility::SqliteStmt stmt_get_bet;
        utility::SqliteStmt stmt_get_all_bet;

        // буфер для записи
		std::deque<BetData> write_buffer;
		std::mutex write_buffer_mutex;
        // для бэкапа
		bool is_backup = ATOMIC_VAR_INIT(false);
		std::mutex backup_mutex;
		// флаг сброса
		std::atomic<bool> is_shutdown = ATOMIC_VAR_INIT(false);
        // уникальный номер сделки
		int64_t last_bet_uid = 0;
        std::mutex last_bet_uid_mutex;

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
                print_error("sqlite3_open_v2 error, db name " + db_name);
                return false;
            }
            sqlite3_busy_timeout(sqlite_db_ptr, config.busy_timeout);
            // создаем таблицу в базе данных, если она еще не создана
            const std::string create_key_value_table_sql(
                "CREATE TABLE IF NOT EXISTS 'Bets' ("
                "uid 			INTEGER NOT NULL,"
                "broker_id 		INTEGER NOT NULL,"
                "open_date 		INTEGER NOT NULL,"
                "close_date 	INTEGER NOT NULL,"
                "open_price 	REAL    NOT NULL,"
                "close_price    REAL    NOT NULL,"
                "amount 		REAL    NOT NULL,"
                "profit 		REAL    NOT NULL,"
                "payout 		REAL    NOT NULL,"
                "delay 		    INTEGER NOT NULL,"
                "ping 			INTEGER NOT NULL,"
                "duration 		INTEGER NOT NULL,"
                "step 			INTEGER NOT NULL,"
                "demo 			INTEGER NOT NULL,"
                "last 			INTEGER NOT NULL,"
                "contract_type 	INTEGER NOT NULL,"
                "status 		INTEGER NOT NULL,"
                "type			INTEGER NOT NULL,"
                "symbol	 		TEXT	NOT NULL,"
                "broker			TEXT	NOT NULL,"
                "currency		TEXT	NOT NULL,"
                "signal			TEXT	NOT NULL,"
                "comment		TEXT	NOT NULL,"
                "user_data		TEXT	NOT NULL,"
                "PRIMARY KEY (open_date, uid))");
            if (!utility::prepare(sqlite_db_ptr, create_key_value_table_sql)) return false;
            std::lock_guard<std::mutex> lock(last_bet_uid_mutex);
            last_bet_uid = sqlite3_last_insert_rowid(sqlite_db_ptr);
            if (last_bet_uid <= 0) last_bet_uid = 1;
            return true;
        }

        bool init_db(const std::string &db_name, const bool readonly = false) {
            if (!open_db(sqlite_db, db_name, readonly)) {
                sqlite3_close_v2(sqlite_db);
                sqlite_db = nullptr;
                return false;
            }
            if (!sqlite_transaction.init(sqlite_db) ||
                !stmt_replace_bet.init(sqlite_db, "INSERT OR REPLACE INTO 'Bets' ("
                    "uid,"
                    "broker_id,"
                    "open_date,"
                    "close_date,"
                    "open_price,"
                    "close_price,"
                    "amount,"
                    "profit,"
                    "payout,"
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
                    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)") ||
                !stmt_get_bet.init(sqlite_db, "SELECT * FROM 'Bets' WHERE open_date == :x AND uid == :y") ||
                !stmt_get_all_bet.init(sqlite_db, "SELECT * FROM 'Bets'")) {
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
                if (sqlite3_bind_int(stmt.get(), index++, static_cast<sqlite3_int>(buffer[i].delay)) != SQLITE_OK) {
                    transaction.rollback();
                    return false;
                }
                if (sqlite3_bind_int(stmt.get(), index++, static_cast<sqlite3_int>(buffer[i].ping)) != SQLITE_OK) {
                    transaction.rollback();
                    return false;
                }
                if (sqlite3_bind_int(stmt.get(), index++, static_cast<sqlite3_int>(buffer[i].duration)) != SQLITE_OK) {
                    transaction.rollback();
                    return false;
                }
                if (sqlite3_bind_int(stmt.get(), index++, static_cast<sqlite3_int>(buffer[i].step)) != SQLITE_OK) {
                    transaction.rollback();
                    return false;
                }
                if (sqlite3_bind_int(stmt.get(), index++, static_cast<sqlite3_int>(buffer[i].demo)) != SQLITE_OK) {
                    transaction.rollback();
                    return false;
                }
                if (sqlite3_bind_int(stmt.get(), index++, static_cast<sqlite3_int>(buffer[i].last)) != SQLITE_OK) {
                    transaction.rollback();
                    return false;
                }
                if (sqlite3_bind_int(stmt.get(), index++, static_cast<sqlite3_int>(buffer[i].contract_type)) != SQLITE_OK) {
                    transaction.rollback();
                    return false;
                }
                if (sqlite3_bind_int(stmt.get(), index++, static_cast<sqlite3_int>(buffer[i].status)) != SQLITE_OK) {
                    transaction.rollback();
                    return false;
                }
                if (sqlite3_bind_int(stmt.get(), index++, static_cast<sqlite3_int>(buffer[i].type)) != SQLITE_OK) {
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
                    print_error("sqlite3_step return SQLITE_BUSY");
                    return false;
                } else {
                    transaction.rollback();
                    print_error("sqlite3_step return " + std::to_string(err) + ", message " + std::string(sqlite3_errmsg(sqlite_db)));
                    return false;
                }
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
                    print_error("sqlite3_step return code " + std::to_string(err));
                    return T();
                }

                err = 0;
                while (true) {
                    typename T::value_type bet_data;

                    size_t index = 0;
                    bet_data.id 			= sqlite3_column_int64(stmt.get(), index++);
                    bet_data.broker_id 		= sqlite3_column_int64(stmt.get(), index++);

                    bet_data.open_date 		= sqlite3_column_double(stmt.get(), index++);
                    bet_data.close_date 	= sqlite3_column_double(stmt.get(), index++);

                    bet_data.open_price 	= sqlite3_column_double(stmt.get(), index++);
                    bet_data.close_price 	= sqlite3_column_double(stmt.get(), index++);

                    bet_data.amount 		= sqlite3_column_double(stmt.get(), index++);
                    bet_data.profit 		= sqlite3_column_double(stmt.get(), index++);
                    bet_data.payout 		= sqlite3_column_double(stmt.get(), index++);

                    bet_data.delay 			= sqlite3_column_double(stmt.get(), index++);
                    bet_data.ping 			= sqlite3_column_double(stmt.get(), index++);

                    bet_data.duration 		= static_cast<uint32_t>sqlite3_column_int(stmt.get(), index++);
                    bet_data.step 			= static_cast<uint32_t>sqlite3_column_int(stmt.get(), index++);
                    bet_data.demo 			= static_cast<bool>(sqlite3_column_int(stmt.get(), index++));
                    bet_data.last 			= static_cast<bool>(sqlite3_column_int(stmt.get(), index++));

                    bet_data.contract_type 	= static_cast<ContractTypes>(sqlite3_column_int(stmt.get(), index++));
                    bet_data.status 		= static_cast<BetStatus>(sqlite3_column_int(stmt.get(), index++));
                    bet_data.type 			= static_cast<BoTypes>(sqlite3_column_int(stmt.get(), index++));

                    bet_data.symbol 	= (const char *)sqlite3_column_text(stmt.get(), index++);
                    bet_data.broker 	= (const char *)sqlite3_column_text(stmt.get(), index++);
                    bet_data.currency 	= (const char *)sqlite3_column_text(stmt.get(), index++);
                    bet_data.signal 	= (const char *)sqlite3_column_text(stmt.get(), index++);
                    bet_data.comment 	= (const char *)sqlite3_column_text(stmt.get(), index++);
                    bet_data.user_data 	= (const char *)sqlite3_column_text(stmt.get(), index++);

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
                    print_error("sqlite3_step return SQLITE_BUSY");
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }
                break;
            }
            return std::move(buffer);
		}

        // основная задача (запись данных вБД) для фонового процесса
        void main_task() {
            async_tasks.create_task([&]() {
                ztime::Timer timer;
                while (!is_shutdown) {
                    // получаем размер буфера для записи
                    size_t write_buffer_size = 0;
                    {
                        std::lock_guard<std::mutex> lock(write_buffer_mutex);
                        write_buffer_size = write_buffer.size();
                    }
                    // замеряем время бездействия
                    const double idle_time = timer.get_elapsed();
                    // проверяем наличие данных для записи
                    if (write_buffer_size == 0) {
                        timer.reset();
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        continue;
                    }
                    // проверяем условия для начала записи
                    if (idle_time < config.idle_time && write_buffer_size < config.threshold_bets)  {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        continue;
                    }

                    // если порог количества ставок превышен
                    // или время бездействия истекло
                    // начинаем запись в БД

                    // скопируем данные
                    std::deque<BetData> buffer;
                    {
                        std::lock_guard<std::mutex> lock(write_buffer_mutex);
                        buffer = write_buffer;
                        write_buffer.clear();
                    }
                    // запишем данные
                    replace_db(buffer, sqlite_transaction, stmt_replace_bet);
                }
            });
        }

        /** \brief Проверить инициализацию БД
         * \return Вернет true если БД было инициализирована
         */
        const bool check_init_db() noexcept {
            return (sqlite_db != nullptr);
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

        template<class T>
        inline bool check_in_list(const T &list_num) {
            if (!list_num.empty()) {
                const uint32_t weekday = ztime::get_weekday(timestamp);
                bool nofilter = false;
                for (size_t k = 0; k < list_num.size(); ++k) {
                    if (request.weekday[k] == weekday) {
                        nofilter = true;
                        break;
                    }
                }
                return nofilter;
            }
        }

	public:

        /** \brief Конструктор хранилища тиков
         * \param path      Путь к файлу
         * \param readonly  Флаг 'только чтение'
         */
		BetDatabase(const std::string &path, const bool readonly = false) :
            database_name(path) {
            if (init_db(path, readonly)) {
                main_task();
            }
		}

		~BetDatabase() {
            is_shutdown = true;
            async_tasks.clear();
            std::lock_guard<std::mutex> lock(method_mutex);
            if (sqlite_db != nullptr) sqlite3_close_v2(sqlite_db);
		}

		bool init(const std::string &path, const bool readonly = false) {
            std::lock_guard<std::mutex> lock(method_mutex);
            if (check_init_db()) return false;
            if (init_db(path, readonly)) {
                main_task();
                return true;
            }
            return false;
		}

        /** \brief Получить уникальный номер сделки
         * \return Вернет уникальный номер сделки
         */
		inline int64_t get_bet_uid() noexcept {
            std::lock_guard<std::mutex> lock(last_bet_uid_mutex);
            const int64_t temp = last_bet_uid;
            ++last_bet_uid;
            return temp;
		}

		/** \brief Добавить или обновить ставку в БД
		 * Метод присвоит уникальный uid самостяотельно, если он не указан
         * \param bet_data  Данные ставки
         * \return Вернет true, если ставка была добавлена в очередь назапись в БД
         */
		bool replace(BetData &bet_data) noexcept {
			{
                std::lock_guard<std::mutex> lock(method_mutex);
                if (!check_init_db()) return false;
			}
			// проверяем данные на валидность
			if (bet_data.open_date <= 0) return false;
            if (bet_data.uid <= 0) bet_data.uid = get_bet_uid();
			std::lock_guard<std::mutex> lock(write_buffer_mutex);
			write_buffer.push_back(data);
			return true;
		}

		/** \brief Параметры запроса ставок из БД
         */
		class RequestConfig {
		public:
			int64_t start_date  = 0;	                /// Начальная дата запроса, в мс
			int64_t stop_date 	= 0;	                /// Конечная дата запроса, в мс
			std::vector<std::string>	brokers;	    /// Список брокеров. Если пусто, то не фильтруется
			std::vector<std::string>	no_brokers;     /// Список запрещенных брокеров. Если пусто, то не фильтруется
			std::vector<std::string> 	symbols;	    /// Список символов. Если пусто, то не фильтруется
			std::vector<std::string> 	no_symbols;	    /// Список запрещенных символов. Если пусто, то не фильтруется
			std::vector<std::string> 	signals;	    /// Список сигналов. Если пусто, то не фильтруется
			std::vector<std::string> 	no_signals;	    /// Список запрещенных сигналов. Если пусто, то не фильтруется
			std::vector<uint32_t> 	 	durations;	    /// Список экспираций. Если пусто, то не фильтруется
			std::vector<uint32_t> 	 	no_durations;   /// Список запрещенных экспираций. Если пусто, то не фильтруется
			std::vector<uint32_t> 	 	hours;		    /// Список торговых часов. Если пусто, то не фильтруется
			std::vector<uint32_t> 	 	no_hours;       /// Список запрещенных торговых часов. Если пусто, то не фильтруется
			std::vector<uint32_t>	 	weekday;	    /// Список торговых дней недели. Если пусто, то не фильтруется
			std::vector<uint32_t>	 	no_weekday;	    /// Список запрещенных торговых дней недели. Если пусто, то не фильтруется
			uint32_t start_time = 0;	                /// Начальное время торговли
			uint32_t stop_time = 0;		                /// Конечное время торговли
		};

		/** \brief Получить данные из БД
         * \param request	Структура запроса, которая определяет фильтрацию данных
         * \return Вернет массив ставок, если есть. Если массив пустой, то данных удовлетворяющих запросу нет
         */
		template<class T>
		inline T get(const RequestConfig &request) noexcept {
			{
                std::lock_guard<std::mutex> lock(method_mutex);
                if (!check_init_db()) return T();
			}
			// формируем запрос
			std::string request_str("SELECT * FROM 'Bets' WHERE");
			int counter_args = 0; //  счетчик аргументов
			if (request.start_date != 0) {
				request_str += " open_date >= " + std::to_string(request.start_date);
				++counter_args;
			}
			if (request.stop_date != 0) {
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
            add_list_num_arg_req(request.durations, "duration", false, counter_args, request_str);
            add_list_num_arg_req(request.no_durations, "duration", true, counter_args, request_str);
			request_str += " ORDER BY id ASC";
			// подготавливаем запрос
			utility::SqliteStmt stmt;
			stmt.init(sqlite_db, request_str.c_str());
            // получаем данные
			T buffer(get_bets_db(stmt));

			// проводим оставшуюся фильтрацию
			size_t index = 0;
			while (index < buffer.size()) {
				// получаем метку времени
				const uint64_t timestamp = buffer[index].open_date / ztime::MILLISECONDS_IN_SECOND;

				// фильтруем по дням недели
				if (!request.weekday.empty()) {
					const uint32_t weekday = ztime::get_weekday(timestamp);
					bool nofilter = false;
					for (size_t k = 0; k < request.weekday.size(); ++k) {
						if (request.weekday[k] == weekday) {
							nofilter = true;
							break;
						}
					}
					if (!nofilter) {
                        buffer.erase(buffer.begin() + index);
                        ++index;
                        continue;
					}
				}
				if (!request.no_weekday.empty()) {
					const uint32_t weekday = ztime::get_weekday(timestamp);
					bool nofilter = true;
					for (size_t k = 0; k < request.no_weekday.size(); ++k) {
						if (request.no_weekday[k] == weekday) {
							nofilter = false;
							break;
						}
					}
					if (!nofilter) {
                        buffer.erase(buffer.begin() + index);
                        ++index;
                        continue;
					}
				}


				const uint32_t weekday = ztime::get_weekday(timestamp);

				// проверяем, была ли команда на удаление
				if (!nofilter) {
					buffer.erase(buffer.begin() + index);
					++index;
					continue;
				}
				const uint64_t timestamp = buffer[index].open_date / ztime::MILLISECONDS_IN_SECOND;

				nofilter = false;
				if (!request.weekday.empty()) {
					const uint32_t weekday = xtime::get_weekday(timestamp_ms/1000);
					for (size_t k = 0; k < request.weekday.size(); ++k) {
						if (request.weekday[k] == weekday) {
							nofilter = true;
							break;
						}
					}
				} else {
					nofilter = true;
				}
				if (!nofilter) continue;
				nofilter = false;
				if (!request.hours.empty()) {
					const uint32_t hour = xtime::get_hour_day(timestamp_ms/1000);
					for (size_t k = 0; k < request.hours.size(); ++k) {
						if (request.hours[k] == hour) {
							nofilter = true;
							break;
						}
					}
				} else {
					nofilter = true;
				}

				if (!nofilter) continue;
				nofilter = false;

				if (request.start_time) {
					const uint32_t second_day = xtime::get_second_day(timestamp_ms/1000);
					if (request.start_time <= second_day) nofilter = true;
				} else {
					nofilter = true;
				}

				if (!nofilter) continue;
				nofilter = false;

				if (request.stop_time) {
					const uint32_t second_day = xtime::get_second_day(timestamp_ms/1000);
					if (request.stop_time >= second_day) nofilter = true;
				} else {
					nofilter = true;
				}

				if (!nofilter) continue;
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
                if (!utility::backup_form_db(path, this->sqlite_db)) {
                    callback(path, true);
                    if (config.use_log) {
                        TRADING_DB_TICK_DB_PRINT
                            << "trading_db::BetDatabase error in [file "
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

        /** \brief Очистить все данные
         */
		inline bool clear() {
            std::lock_guard<std::mutex> lock(stmt_mutex);
            return utility::prepare(sqlite_db, "DELETE FROM 'Bets'");
		}
	};

}; // trading_db

#endif // TRADING_DB_KEY_VALUE_DATABASE_HPP_INCLUDED
