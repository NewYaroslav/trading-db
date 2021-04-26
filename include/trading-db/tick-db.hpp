#pragma once
#ifndef TRADING_DB_TICK_DB_HPP
#define TRADING_DB_TICK_DB_HPP

#if SQLITE_THREADSAFE != 2
//#error "The project must be built for sqlite multithreading! Set the SQLITE_THREADSAFE=2"
#endif

#include "config.hpp"
#include "utility/async-tasks.hpp"
#include "utility/print.hpp"
#include <sqlite_orm/sqlite_orm.h>
#include <xtime.hpp>
#include <mutex>
#include <atomic>
#include <future>

namespace trading_db {

    /** \brief Класс базы данных тика
     */
	class TickDb {
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

        using Period = xtime::period_t;

        class EndTickStamp {
        public:
            uint64_t timestamp = 0;

            EndTickStamp() {};
            EndTickStamp(const uint64_t t) : timestamp(t) {};
        };

        /** \brief Структура настроек
         */
        class Note {
        public:
            std::string key;
            std::string value;
        };

    private:
        std::string db_file_name;

        const size_t wait_delay = 10;
        const size_t wait_process_delay = 500;
        size_t write_buffer_size = 1000;    /**< Размер буфера для записи */
        size_t write_buffer_index = 0;

        utility::AsyncTasks async_tasks;

        std::deque<Tick> write_buffer;
        std::deque<EndTickStamp> write_end_tick_buffer;
        EndTickStamp write_end_tick_stamp;
        std::mutex write_buffer_mutex;
        uint64_t write_buffer_restart_size = 0;

        std::deque<Tick> read_buffer;
        std::deque<EndTickStamp> read_buffer_end_tick_stamp;
        Period read_period;
        std::mutex read_buffer_mutex;

		uint64_t indent_past = xtime::MILLISECONDS_IN_SECOND * xtime::SECONDS_IN_HOUR;
		uint64_t indent_future = xtime::MILLISECONDS_IN_SECOND * xtime::SECONDS_IN_HOUR;

		std::atomic<bool> is_backup = ATOMIC_VAR_INIT(false);
		std::atomic<bool> is_recording = ATOMIC_VAR_INIT(false);
		std::atomic<bool> is_stop_write = ATOMIC_VAR_INIT(false);
		std::atomic<bool> is_block_write = ATOMIC_VAR_INIT(false);
		std::atomic<bool> is_restart_write = ATOMIC_VAR_INIT(false);
		std::atomic<bool> is_writing = ATOMIC_VAR_INIT(false);
		std::atomic<bool> is_shutdown = ATOMIC_VAR_INIT(false);
        std::atomic<bool> is_stop = ATOMIC_VAR_INIT(false);

        std::atomic<uint32_t> wait_stop_timer = ATOMIC_VAR_INIT(60);
        std::atomic<uint32_t> wait_stop_timestamp = ATOMIC_VAR_INIT(0);

        std::mutex backup_mutex;

        /** \brief Получить размер буфера для записи
         */
		inline size_t get_write_buffer_size() noexcept {
            std::lock_guard<std::mutex> lock(write_buffer_mutex);
            return write_buffer.size();
		}

		inline void create_protected(
                const std::function<void()> &callback,
                const std::function<void(const uint64_t counter)> &callback_error,
                const uint64_t delay = 500) {
            uint64_t counter = 0;
            while (!is_shutdown) {
                try {
                    //std::lock_guard<std::mutex> lock(backup_mutex);
                    callback();
                    break;
                } catch(...) {
                    ++counter;
                    if (callback_error != nullptr) callback_error(counter);
                    std::this_thread::sleep_for(std::chrono::milliseconds(delay));
                    continue;
                }
            }
		}

		inline void create_protected_transaction(
                const std::function<void()> &callback,
                const std::function<void(const uint64_t counter)> &callback_error,
                const uint64_t delay = 500) {
            uint64_t counter = 0;
            while (!is_shutdown) {
                try {
                    //std::lock_guard<std::mutex> lock(backup_mutex);
                    storage.transaction([&, callback]() mutable {
                        callback();
                        return true;
                    });
                    break;
                } catch(...) {
                    ++counter;
                    if (callback_error != nullptr) callback_error(counter);
                    std::this_thread::sleep_for(std::chrono::milliseconds(delay));
                    continue;
                }
            }
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

        /* столбец таблицы */
        template<class O, class T, class ...Op>
        using Column = sqlite_orm::internal::column_t<O, T, const T& (O::*)() const, void (O::*)(T), Op...>;

        /* тип хранилища
         * Внимание!
         * Компилятор mingw вылетает, когда столбцов в таблице становится от 15 шт.
         */
        using Storage = sqlite_orm::internal::storage_t<
            sqlite_orm::internal::table_t<Tick,
                Column<Tick, decltype(Tick::timestamp), sqlite_orm::constraints::primary_key_t<>>,
                Column<Tick, decltype(Tick::server_timestamp)>,
                Column<Tick, decltype(Tick::bid)>,
                Column<Tick, decltype(Tick::ask)>
            >,
            sqlite_orm::internal::table_t<EndTickStamp,
                Column<EndTickStamp, decltype(EndTickStamp::timestamp), sqlite_orm::constraints::primary_key_t<>>
            >,
            sqlite_orm::internal::table_t<Note,
                Column<Note, decltype(Note::key), sqlite_orm::constraints::primary_key_t<>>,
                Column<Note, decltype(Note::value)>
            >
        >;

		Storage storage;    /**< БД sqlite */

        /** \brief Инициализировать хранилище
         * \bug Компилятор mingw вылетает, когда столбцов в таблице становится от 15 шт.
         * \param path Путь к БД
         * \return Возвращает базу данных sqlite orm
         */
        inline auto init_storage(const std::string &path) {
            return sqlite_orm::make_storage(path,
                sqlite_orm::make_table("Ticks",
                    sqlite_orm::make_column("timestamp", &Tick::timestamp, sqlite_orm::primary_key()),
                    sqlite_orm::make_column("server_timestamp", &Tick::server_timestamp),
                    sqlite_orm::make_column("bid", &Tick::bid),
                    sqlite_orm::make_column("ask", &Tick::ask)),
                sqlite_orm::make_table("EndTickStamp",
                    sqlite_orm::make_column("timestamp", &EndTickStamp::timestamp, sqlite_orm::primary_key())),
                sqlite_orm::make_table("Note",
                    sqlite_orm::make_column("key", &Note::key, sqlite_orm::primary_key()),
                    sqlite_orm::make_column("value", &Note::value)));
		}

		bool checkpoint_reusing_db()  {
            //sqlite3* database = storage.get_connection();
            bool retVal = true;
            int  status;

            const int kSQLiteCheckpointMode = SQLITE_CHECKPOINT_PASSIVE;

            //status = storage.wal_checkpoint();//sqlite3_wal_checkpoint_v2(database, NULL, kSQLiteCheckpointMode, NULL, NULL);

            if (status != SQLITE_OK) {
                //printf("gbDB_Meta_PRAGMA_Journal_WAL_ReusingDB: [ERR] Checkpoint failed, msg: %s ... status: %d\n", sqlite3_errmsg(database), status);
                printf("gbDB_Meta_PRAGMA_Journal_WAL_ReusingDB: [ERR] Checkpoint failed, status: %d\n", status);
                uint32_t        currentSleepMS  = 0;
                const uint32_t kSleepIntervalMS = 100;                      // 100ms
                const uint32_t kSleepIntervalUS = kSleepIntervalMS * 1000;  // 100ms -> us
                const uint32_t kMaxSleepMS      = 60 * 1000;                // 60s   -> ms


                while (status != SQLITE_OK)
                {
                    if (          (status != SQLITE_BUSY
                                && status != SQLITE_LOCKED)
                        || currentSleepMS >= kMaxSleepMS)
                    {
                        break;
                    }//if

                    //status = sqlite3_wal_checkpoint_v2(database, NULL, kSQLiteCheckpointMode, NULL, NULL);
                    //status = storage.wal_checkpoint();

                    std::this_thread::sleep_for(std::chrono::milliseconds(kSleepIntervalUS));
                    currentSleepMS += kSleepIntervalMS;
                }//while

                if (currentSleepMS >= kMaxSleepMS)
                {
                    printf("gbDB_Meta_PRAGMA_Journal_WAL_ReusingDB: [ERR] Timeout while DB was busy.\n");
                }//if

                if (status != SQLITE_OK)
                {
                    printf("gbDB_Meta_PRAGMA_Journal_WAL_ReusingDB: [ERR] Checkpoint failed,... status: %d\n", status);
                    retVal = false;;
                }//if
            }//if

            return retVal;
        }

        /** \brief Инициализация объектов класса
         */
		inline void init_other() noexcept {
            using namespace sqlite_orm;
            /* если вы хотите получить доступ к хранилищу из разных потоков,
             * вы должны вызвать его open_forever сразу после создания хранилища,
             * потому что хранилище может создавать разные соединения
             * в результате гонок данных внутри хранилища
             * https://github.com/fnc12/sqlite_orm/issues/163
             */
            {
            std::lock_guard<std::mutex> lock(backup_mutex);
            storage.sync_schema();
            storage.open_forever();
            //storage.pragma.journal_mode(journal_mode::WAL);
            storage.busy_timeout(0);
            }

            //sqlite3_enable_shared_cache(1);

            async_tasks.create_task([&]() {
                TRADING_DB_TICK_DB_PRINT << db_file_name << "---Storage 1" << std::endl;
                Storage writing_storage(init_storage(db_file_name));
                {
                    std::lock_guard<std::mutex> lock(backup_mutex);
                    TRADING_DB_TICK_DB_PRINT << db_file_name << "---Storage 2" << std::endl;
                    writing_storage.sync_schema();
                    //writing_storage.pragma.journal_mode(journal_mode::WAL);
                    TRADING_DB_TICK_DB_PRINT << db_file_name << "---Storage 3" << std::endl;
                    writing_storage.open_forever();
                    writing_storage.busy_timeout(0);
                    TRADING_DB_TICK_DB_PRINT << db_file_name << "---Storage 4" << std::endl;
                }

                uint64_t stop_timestamp = 0;
                bool is_stop_timestamp = false;
                while (true) {
                    bool is_write_buffer = false;
                    bool is_write_end_tick_stamp = false;
                    // данные для записи
                    std::deque<Tick> transaction_tick_buffer;
                    std::deque<EndTickStamp> transaction_end_tick_buffer;

                    // запоминаем данные для записи
                    {
                        std::lock_guard<std::mutex> lock(write_buffer_mutex);
                        if (xtime::get_timestamp() > wait_stop_timestamp) is_stop_write = true;
                        if (write_buffer.size() > write_buffer_size || is_stop_write) {
                            if (!write_buffer.empty()) {
                                transaction_tick_buffer.resize(write_buffer.size());
                                transaction_tick_buffer.assign(write_buffer.begin(), write_buffer.end());
                            }
                            if (!write_end_tick_buffer.empty()) {
                                transaction_end_tick_buffer.resize(write_end_tick_buffer.size());
                                transaction_end_tick_buffer.assign(write_end_tick_buffer.begin(), write_end_tick_buffer.end());
                            }
                        }
                    }

                    if (!transaction_tick_buffer.empty() || !transaction_end_tick_buffer.empty()) {
                        create_protected([&](){
                            std::lock_guard<std::mutex> lock(backup_mutex);
                            writing_storage.begin_transaction();
                            if (!transaction_tick_buffer.empty()) {
                                for (const Tick& t : transaction_tick_buffer) {
                                    writing_storage.replace(t);
                                }
                            }
                            if (!transaction_end_tick_buffer.empty()) {
                                for (const EndTickStamp& t : transaction_end_tick_buffer) {
                                    writing_storage.replace(t);
                                }
                            }
                            writing_storage.commit();
                        },[&](const uint64_t error_counter){
                            TRADING_DB_TICK_DB_PRINT
                                << "trading-db: " << db_file_name
                                << " sqlite transaction error! Line " << __LINE__ << ", counter = "
                                << error_counter << std::endl;
                        });
                    }

                    {
                        std::lock_guard<std::mutex> lock(write_buffer_mutex);
                        /* очищаем буфер с котировками */
                        if (!write_buffer.empty() && !transaction_tick_buffer.empty()) {
                            write_buffer.erase(write_buffer.begin(), write_buffer.begin() + transaction_tick_buffer.size());
                        }
                        if (!write_end_tick_buffer.empty() && !transaction_end_tick_buffer.empty()) {
                            write_end_tick_buffer.erase(write_end_tick_buffer.begin(), write_end_tick_buffer.begin() + transaction_end_tick_buffer.size());
                        }
                        if (write_buffer.empty()) is_writing = false;
                    }
                    if (is_shutdown) break;
                    std::this_thread::sleep_for(std::chrono::milliseconds(wait_delay));
                }
                is_stop = true;
                TRADING_DB_TICK_DB_PRINT << db_file_name << " stop" << std::endl;
            });
		}

        /** \brief Прочитать данные тиков на заданном промежутке времени из БД в буфер
         */
		void read_db_to_buffer(
                std::deque<Tick> &buffer,
                std::deque<EndTickStamp> &buffer_end_tick_stamp,
                Period &period,
                const uint64_t timestamp_ms) noexcept {
            using namespace sqlite_orm;
            period.start = timestamp_ms > indent_past ? timestamp_ms - indent_past : 0;
            period.stop = timestamp_ms + indent_future;
            if (!buffer.empty()) buffer.clear();
            if (!buffer_end_tick_stamp.empty()) buffer_end_tick_stamp.clear();

            /* сначала загружаем тиковые данные */
            {
                std::deque<Tick> beg_element, end_element;
                create_protected([&](){
                    beg_element = storage.get_all<Tick, std::deque<Tick>>(
                        where(c(&Tick::timestamp) <= period.start),
                        order_by(&Tick::timestamp).desc(),
                        limit(1));
                },[&](const uint64_t error_counter){
                    TRADING_DB_TICK_DB_PRINT << "trading-db: " << db_file_name
                    << " sqlite get_all error! Line " << __LINE__ << ", counter = "
                    << error_counter << std::endl;
                });

                create_protected([&](){
                    buffer = storage.get_all<Tick, std::deque<Tick>>(where(
                            c(&Tick::timestamp) < period.stop &&
                            c(&Tick::timestamp) > period.start));
                },[&](const uint64_t error_counter){
                    TRADING_DB_TICK_DB_PRINT << "trading-db: " << db_file_name
                    << " sqlite get_all error! Line " << __LINE__ << ", counter = "
                    << error_counter << std::endl;
                },500);

                create_protected([&](){
                    end_element = storage.get_all<Tick, std::deque<Tick>>(
                        where(c(&Tick::timestamp) >= period.stop),
                        order_by(&Tick::timestamp).asc(),
                        limit(1));
                },[&](const uint64_t error_counter){
                    TRADING_DB_TICK_DB_PRINT << "trading-db: " << db_file_name
                        << " sqlite get_all error! Line " << __LINE__ << ", counter = "
                        << error_counter << std::endl;
                },500);

                if (!beg_element.empty()) buffer.push_front(beg_element.back());
                if (!end_element.empty()) buffer.push_back(end_element.front());
            }

            /* загружаем метки остановки тиковых данных */
            if (!buffer.empty()) {
                Period result_period(buffer.front().timestamp, buffer.back().timestamp);
                create_protected([&](){
                    buffer_end_tick_stamp = storage.get_all<EndTickStamp, std::deque<EndTickStamp>>(where(
                            c(&EndTickStamp::timestamp) <= result_period.stop &&
                            c(&EndTickStamp::timestamp) >= result_period.start));
                },[&](const uint64_t error_counter){
                    TRADING_DB_TICK_DB_PRINT << "trading-db: " << db_file_name
                        << " sqlite get_all error! Line " << __LINE__ << ", counter = "
                        << error_counter << std::endl; });
            }
		}

		/** \brief Прочитать данные периодов за заданный промежуток времени из БД в буфер
         */
         /*
		void read_db_to_buffer(std::deque<Period> &buffer, const Period &period) noexcept {
            using namespace sqlite_orm;
            if (!buffer.empty()) buffer.clear();
            create_protected_transaction([&](){
                buffer = storage.get_all<Period, std::deque<Period>>(where(
                        (c(&Period::start) >= period.start &&
                        c(&Period::start) <= period.stop) ||
                        (c(&Period::stop) >= period.start &&
                        c(&Period::stop) <= period.stop) ||
                        (c(&Period::start) <= period.start &&
                        c(&Period::stop) >= period.stop)));
            },[&](const uint64_t error_counter){ std::cerr << "trading-db: sqlite transaction error! Label 0x0B, Counter = " << error_counter << std::endl; });
		}
		*/

	public:

        /** \brief Конструктор хранилища тиков
         * \param path Путь к файлу
         */
		TickDb(const std::string &path) :
                db_file_name(path), storage(init_storage(path)) {
            init_other();
		}

		~TickDb() {
            // ждем завершения бэкапа
            while(is_backup) {
                std::this_thread::sleep_for(std::chrono::milliseconds(wait_delay));
            }

            is_block_write = true;
            // ставим флаг остановки записи
            {
                std::lock_guard<std::mutex> lock(write_buffer_mutex);
                is_stop_write = true;
            }
            wait();
            is_shutdown = true;
            // ждем завершение потоков
            while(!is_stop) {
                std::this_thread::sleep_for(std::chrono::milliseconds(wait_delay));
            }
            async_tasks.clear();
		}

        /** \brief Записать новый тик
         * \param new_tick  Новый тик
         * \return Вернет true в случае успешной записи
         */
        bool write(const Tick& new_tick) noexcept {
            // флаг блокировки 'is_block_write' записи имеет высокий приоритет
            if (is_block_write) return false;
            reset_stop_counter();
            std::lock_guard<std::mutex> lock(write_buffer_mutex);
            // проверяем остановку записи
            if (is_stop_write) return false;
            // ставим флаг записи
            is_recording = true;
            // ставим флаг процесса записи в БД
            is_writing = true;

            if (write_buffer.empty()) {
                write_buffer.push_back(new_tick);
            } else
            if (new_tick.timestamp > write_buffer.back().timestamp) {
                write_buffer.push_back(new_tick);
            } else
            if (new_tick.timestamp == write_buffer.back().timestamp) {
                write_buffer.back() = new_tick;
            } else {
                write_end_tick_buffer.push_back(write_end_tick_stamp.timestamp);
                write_buffer.push_back(new_tick);
            }
            write_end_tick_stamp.timestamp = new_tick.timestamp;
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
            write_end_tick_buffer.push_back(write_end_tick_stamp.timestamp);
            is_stop_write = true;
            return true;
		}

		inline void set_wait_stop_time(const uint32_t wait_time) {
            wait_stop_timer = wait_time;
		}

		inline void reset_stop_counter() {
            const uint64_t temp = wait_stop_timer + xtime::get_timestamp();
            wait_stop_timestamp = temp;
		}

		Tick get(const uint64_t timestamp_ms, const bool db_only = false) noexcept {
            if (!db_only) {
                // ищем данные в буфере
                int state = 0;
                std::deque<Tick> buffer_1;
                std::deque<Tick> buffer_2;
                uint64_t end_tick_stamp = 0;
                {
                    std::lock_guard<std::mutex> lock(write_buffer_mutex);
                    if (!write_buffer.empty()) {
                        end_tick_stamp = write_end_tick_stamp.timestamp;
                        if (is_restart_write) {
                            /* в этом случае есть два периода */
                            state = 1;
                            /* лень переписывать функции, сделаем по тупому */
                            buffer_1.resize(write_buffer_restart_size);
                            buffer_2.resize(write_buffer.size() - write_buffer_restart_size);
                            buffer_1.assign(write_buffer.begin(), write_buffer.begin() + write_buffer_restart_size);
                            buffer_2.assign(write_buffer.begin() + write_buffer_restart_size, write_buffer.end());
                        } else {
                            /* в данном варианте один период */
                            buffer_1.resize(write_buffer.size());
                            buffer_1.assign(write_buffer.begin(), write_buffer.end());
                            if (!is_stop_write) state = 2;
                            else state = 3;
                        }
                    }
                }
                switch(state) {
                case 0:
                    break;
                case 1: {
                        Tick tick_2 = find_for_timestamp(buffer_2, timestamp_ms);
                        if (!tick_2.empty()) return tick_2;

                        Tick tick_1 = find_for_timestamp(buffer_1, timestamp_ms);
                        if (!tick_1.empty()) {
                            /* проверяем, является ли тик последним */
                            if (end_tick_stamp == tick_1.timestamp) return Tick();
                            return tick_1;
                        }
                    } break;
                case 2:
                case 3: {
                        Tick tick_1 = find_for_timestamp(buffer_1, timestamp_ms);
                        if (!tick_1.empty()) {
                            if (state == 3) {
                                /* проверяем, является ли тик последним */
                                if (end_tick_stamp == tick_1.timestamp) return Tick();
                            }
                            return tick_1;
                        }
                    } break;
                };
            } // if

            // далее идет работа с базой данных, ведь буфер не содержит искомых данных
            {
                std::lock_guard<std::mutex> lock(read_buffer_mutex);
                if (read_buffer.empty() ||
                    read_period.start >= timestamp_ms ||
                    read_period.stop <= timestamp_ms) {
                    /* загружаем тики и метки конца тиковых данных */
                    read_db_to_buffer(read_buffer, read_buffer_end_tick_stamp, read_period, timestamp_ms);
                }
                if (!read_buffer.empty()) {
                    Tick tick = find_for_timestamp(read_buffer, timestamp_ms);
                    EndTickStamp stamp = find_for_timestamp(read_buffer_end_tick_stamp, timestamp_ms);
                    if (!tick.empty()) {
                        /* проверяем, является ли тик последним */
                        if (stamp.timestamp == tick.timestamp) return Tick();
                    }
                    return tick;
                }
            }
            return Tick();
		}

		xtime::period_t get_data_period() {
            xtime::period_t data_period;
            data_period.start = 0;
            data_period.stop = 0;
            create_protected_transaction([&](){
                if(auto start_date = storage.min(&Tick::timestamp)){
                    data_period.start = *start_date;
                }
                if(auto stop_date = storage.max(&Tick::timestamp)){
                    data_period.stop = *stop_date;
                }
            },[&](const uint64_t error_counter){
                TRADING_DB_TICK_DB_PRINT << "trading-db: " << db_file_name
                << " sqlite get_all error! Line " << __LINE__ << ", counter = "
                << error_counter << std::endl;
            });
            data_period.start /= xtime::MILLISECONDS_IN_SECOND;
            data_period.stop /= xtime::MILLISECONDS_IN_SECOND;
            return data_period;
		}

		private:
		// переменные для работы с get_first_upper
		std::vector<Tick> first_upper_buffer_2;
        xtime::period_t first_upper_period_2;
		std::vector<Tick> first_upper_buffer_1;
		xtime::period_t first_upper_period_1;
		size_t first_upper_buffer_size = 10000;
		size_t first_upper_buffer_index = 0;

		public:

        /** \brief Получить первый ближайший тик выше указанной метки времени
         * \param timestamp_ms      Метка времени
         * \param use_server_time   Тип времени, если true, используется время сервера
         * \return Тик, время которого равно или больше указанной метки времени
         */
		inline Tick get_first_upper(const uint64_t timestamp_ms, const bool use_server_time = false) {
            using namespace sqlite_orm;
            if (use_server_time) {
                if (first_upper_buffer_2.empty() ||
                    timestamp_ms < first_upper_period_2.start ||
                    timestamp_ms > first_upper_period_2.stop) {

                    first_upper_period_2.start = timestamp_ms;
                    first_upper_buffer_2 = get_first_upper_array(timestamp_ms, first_upper_buffer_size, true);
                    if (first_upper_buffer_2.empty()) first_upper_period_2.stop = timestamp_ms;
                    else first_upper_period_2.stop = first_upper_buffer_2.back().server_timestamp;
                }

                if (!first_upper_buffer_2.empty()) {
                    auto it = std::lower_bound(
                        first_upper_buffer_2.begin(),
                        first_upper_buffer_2.end(),
                        timestamp_ms,
                        [](const Tick &lhs, const uint64_t &timestamp) {
                        return lhs.server_timestamp < timestamp;
                    });
                    if (it == first_upper_buffer_2.end()) return Tick();
                    return *it;
                }
                return Tick();
            } else {
                if (first_upper_buffer_1.empty() ||
                    timestamp_ms < first_upper_period_1.start ||
                    timestamp_ms > first_upper_period_1.stop) {

                    first_upper_period_1.start = timestamp_ms;
                    first_upper_buffer_1 = get_first_upper_array(timestamp_ms, first_upper_buffer_size, false);
                    if (first_upper_buffer_1.empty()) first_upper_period_1.stop = timestamp_ms;
                    else first_upper_period_1.stop = first_upper_buffer_1.back().timestamp;
                }

                if (!first_upper_buffer_1.empty()) {
                    auto it = std::lower_bound(
                        first_upper_buffer_1.begin(),
                        first_upper_buffer_1.end(),
                        timestamp_ms,
                        [](const Tick &lhs, const uint64_t &timestamp) {
                        return lhs.timestamp < timestamp;
                    });
                    if (it == first_upper_buffer_1.end()) return Tick();
                    return *it;
                }
                return Tick();
            }
            return Tick();
		}

#if(0)
		inline Tick get_first_upper(const uint64_t timestamp_ms, const bool use_server_time = false) {
            using namespace sqlite_orm;
            std::vector<Tick> first_tick;
            if (use_server_time) {
                first_tick = storage.get_all<Tick>(
                        where(c(&Tick::server_timestamp) >= timestamp_ms),
                        order_by(&Tick::server_timestamp).asc(),
                        limit(1));
            } else {
                first_tick = storage.get_all<Tick>(
                        where(c(&Tick::timestamp) >= timestamp_ms),
                        order_by(&Tick::timestamp).asc(),
                        limit(1));
            }
            return first_tick.empty() ? Tick() : first_tick[0];
		}
#endif

        /** \brief Получить массив ближайших тиков выше указанной метки времени
         * \param timestamp_ms      Метка времени
         * \param length            Длина массива
         * \param use_server_time   Тип времени, если true, используется время сервера
         * \return Массив тиков, время которых равно или больше указанной метки времени
         */
		inline std::vector<Tick> get_first_upper_array(const uint64_t timestamp_ms, const size_t length, const bool use_server_time = false) {
            using namespace sqlite_orm;
            std::vector<Tick> first_array;
            if (use_server_time) {
                first_array = storage.get_all<Tick>(
                        where(c(&Tick::server_timestamp) >= timestamp_ms),
                        order_by(&Tick::server_timestamp).asc(),
                        limit(length));
            } else {
                first_array = storage.get_all<Tick>(
                        where(c(&Tick::timestamp) >= timestamp_ms),
                        order_by(&Tick::timestamp).asc(),
                        limit(length));
            }
            if (first_array.size() < length) first_array.resize(length, Tick());
            return first_array;
		}

        /** \brief Получить первый ближайший тик ниже указанной метки времени
         * \param timestamp_ms      Метка времени
         * \param use_server_time   Тип времени, если true, используется время сервера
         * \return Тик, время которого равно или меньше указанной метки времени
         */
		inline Tick get_first_lower(const uint64_t timestamp_ms, const bool use_server_time = false) {
            using namespace sqlite_orm;
            std::vector<Tick> first_tick;
            create_protected([&](){
                if (use_server_time) {
                    std::lock_guard<std::mutex> lock(backup_mutex);
                    first_tick = storage.get_all<Tick>(
                            where(c(&Tick::server_timestamp) <= timestamp_ms),
                            order_by(&Tick::server_timestamp).desc(),
                            limit(1));
                } else {
                    std::lock_guard<std::mutex> lock(backup_mutex);
                    TRADING_DB_TICK_DB_PRINT << "trading-db: " << db_file_name << " get_first_lower" << std::endl;
                    first_tick = storage.get_all<Tick>(
                            where(c(&Tick::timestamp) <= timestamp_ms),
                            order_by(&Tick::timestamp).desc(),
                            limit(1));
                    TRADING_DB_TICK_DB_PRINT << "trading-db: " << db_file_name << " get_first_lower end" << std::endl;
                }
            },[&](const uint64_t error_counter){
                TRADING_DB_TICK_DB_PRINT
                                << "trading-db: " << db_file_name
                                << " sqlite get_all error! Line " << __LINE__ << ", counter = "
                                << error_counter << std::endl;
            });
            return first_tick.empty() ? Tick() : first_tick[0];
		}

		/** \brief Получить массив ближайших тиков ниже указанной метки времени
         * \param timestamp_ms      Метка времени
         * \param length            Длина массива
         * \param use_server_time   Тип времени, если true, используется время сервера
         * \return Массив тиков, время которых равно или меньше указанной метки времени
         */
		inline std::vector<Tick> get_first_lower_array(const uint64_t timestamp_ms, const size_t length, const bool use_server_time = false) {
            using namespace sqlite_orm;
            std::vector<Tick> first_array;
            if (use_server_time) {
                first_array = storage.get_all<Tick>(
                        where(c(&Tick::server_timestamp) <= timestamp_ms),
                        order_by(&Tick::server_timestamp).desc(),
                        limit(length));
            } else {
                first_array = storage.get_all<Tick>(
                        where(c(&Tick::timestamp) <= timestamp_ms),
                        order_by(&Tick::timestamp).desc(),
                        limit(length));
            }
            if (first_array.size() < length) first_array.resize(length, Tick());
            return first_array;
		}

        /** \brief Починка данных
         * \param wait_tick_ms Время ожидания тика
         */
		void fix_data(const uint32_t wait_tick_ms) {
            xtime::period_t data_period = get_data_period();
            data_period.start *= xtime::MILLISECONDS_IN_SECOND;
            data_period.stop *= xtime::MILLISECONDS_IN_SECOND;
            data_period.stop += wait_tick_ms;
            xtime::timestamp_t last_timestamp = data_period.start;
            xtime::timestamp_t end_timestamp = 0;
            std::vector<EndTickStamp> array_fix;
            for(xtime::timestamp_t t = data_period.start; t <= data_period.stop; ++t) {
                using namespace sqlite_orm;
                std::vector<Tick> beg_tick = storage.get_all<Tick>(
                        where(c(&Tick::timestamp) >= t),
                        order_by(&Tick::timestamp).asc(),
                        limit(1));

                if (beg_tick.empty()) {
                    /*
                    if (last_timestamp == data_period.start) continue;
                    const xtime::timestamp_t diff = t - last_timestamp;
                    if (diff >= wait_tick_ms) {
                        if (end_timestamp == last_timestamp) continue;
                        if(auto end_tick_stamp = storage.get_pointer<EndTickStamp>(last_timestamp)){
                            end_timestamp = last_timestamp;
                        } else {
                            if (!array_fix.empty() && array_fix.back().timestamp == last_timestamp) continue;
                            std::cout << "error! " << xtime::get_str_date_time(last_timestamp / xtime::MILLISECONDS_IN_SECOND) << std::endl;
                            array_fix.push_back(EndTickStamp(last_timestamp));
                        }
                    }
                    */
                } else {

                    const xtime::timestamp_t diff = beg_tick[0].timestamp - last_timestamp;
                    if (diff >= wait_tick_ms) {
                        if (end_timestamp == last_timestamp) continue;
                        if(auto end_tick_stamp = storage.get_pointer<EndTickStamp>(last_timestamp)){
                            end_timestamp = last_timestamp;
                        } else {
                            if (!array_fix.empty() && array_fix.back().timestamp == last_timestamp) continue;
                            std::cout << "error! " << xtime::get_str_date_time(last_timestamp / xtime::MILLISECONDS_IN_SECOND) << std::endl;
                            array_fix.push_back(EndTickStamp(last_timestamp));
                        }
                    }
                    std::cout << xtime::get_str_date_time_ms((double)t / (double)xtime::MILLISECONDS_IN_SECOND) << " diff " << ((double)diff / (double)xtime::MILLISECONDS_IN_SECOND) << std::endl;
                    last_timestamp = beg_tick[0].timestamp;
                    t = last_timestamp;
                    end_timestamp = 0;
                }

                /*
                if(auto tick = storage.get_pointer<Tick>(t)){
                    last_timestamp = tick->timestamp;
                } else {
                    const xtime::timestamp_t diff = t - last_timestamp;
                    if (diff >= wait_tick_ms) {
                        if(auto end_tick_stamp = storage.get_pointer<EndTickStamp>(last_timestamp)){

                        } else {
                            if (!array_fix.empty() && array_fix.back().timestamp == last_timestamp) continue;
                            std::cout << "error! " << xtime::get_str_date_time(last_timestamp / xtime::MILLISECONDS_IN_SECOND) << std::endl;
                            array_fix.push_back(EndTickStamp(last_timestamp));
                        }
                    }
                }
                */
                if (t % 60000 == 0) std::cout << xtime::get_str_date_time(t / xtime::MILLISECONDS_IN_SECOND) << std::endl;

            } // for t
            std::cout << "fixed " << array_fix.size() << std::endl;
            create_protected_transaction([&](){
                for (const EndTickStamp& i : array_fix) {
                    storage.replace(i);
                }
            },[&](const uint64_t error_counter){
                TRADING_DB_TICK_DB_PRINT
                    << "trading-db: " << db_file_name
                    << " sqlite transaction error! Line " << __LINE__ << ", counter = "
                    << error_counter << std::endl;
            });
		}

		void backtesting(
                xtime::period_t data_period,
                const std::function<void(const Tick &tick, const bool is_stop)> &callback) {
            data_period.start *= xtime::MILLISECONDS_IN_SECOND;
            data_period.stop *= xtime::MILLISECONDS_IN_SECOND;
            xtime::timestamp_t last_timestamp = data_period.start;
            xtime::timestamp_t end_timestamp = 0;
            std::vector<EndTickStamp> array_fix;
            for(xtime::timestamp_t t = data_period.start; t <= data_period.stop; ++t) {
                using namespace sqlite_orm;
                std::vector<Tick> first_tick = storage.get_all<Tick>(
                        where(c(&Tick::timestamp) >= t),
                        order_by(&Tick::timestamp).asc(),
                        limit(1));

                if (!first_tick.empty()) {
                    t = last_timestamp = first_tick[0].timestamp;
                    if (last_timestamp > end_timestamp || end_timestamp == 0) {
                        std::vector<EndTickStamp> first_end = storage.get_all<EndTickStamp>(
                            where(c(&EndTickStamp::timestamp) >= t),
                            order_by(&EndTickStamp::timestamp).asc(),
                            limit(1));
                        if (!first_end.empty()) {
                            end_timestamp = first_end[0].timestamp;
                        } else {
                            //?
                        }
                    }
                    callback(first_tick[0], (last_timestamp == end_timestamp));
                }
            } // for t
		}

		template<class T>
		static void backtesting(
                T &array_db,
                xtime::period_t data_period,
                const std::function<void(
                    const size_t index_db,
                    const Tick &tick)> &callback) {
            data_period.start *= xtime::MILLISECONDS_IN_SECOND;
            data_period.stop *= xtime::MILLISECONDS_IN_SECOND;
            std::vector<uint64_t> db_time(array_db.size(), data_period.start);
            std::vector<Tick> ticks(array_db.size());
            std::vector<Tick> ticks_next(array_db.size());
            // начальная инициализация массивов
            size_t index = 0;
            for(auto &item : array_db) {
                std::vector<Tick> temp = item->get_first_upper_array(db_time[index], 2);
                ticks[index] = temp[0];
                ticks_next[index] = temp[1];
                if (ticks_next[index].empty()) {
                    db_time[index] = data_period.stop;
                } else {
                    db_time[index] = ticks_next[index].timestamp + 1;
                }
                ++index;
            } // for
            while(true) {
                // загрузили тики и обновлили время поиска следующего тика
                index = 0;
                for(auto &item : array_db) {
                    if (db_time[index] == data_period.stop) {
                        if (ticks[index].empty()) {
                            ticks[index] = ticks_next[index];
                        }
                        ++index;
                        continue;
                    }
                    if (ticks[index].empty()) {
                        ticks[index] = ticks_next[index];
                        ticks_next[index] = item->get_first_upper(db_time[index]);
                        if (ticks_next[index].empty()) {
                            db_time[index] = data_period.stop;
                        } else {
                            db_time[index] = ticks_next[index].timestamp + 1;
                        }
                    }
                    ++index;
                }

                bool is_exit = true;
                uint64_t min_next_timestamp = data_period.stop;
                for (auto it : ticks_next) {
                    if (it.timestamp != 0 && min_next_timestamp >= it.timestamp) {
                        min_next_timestamp = it.timestamp;
                        is_exit = false;
                    }
                }

                bool is_exit2 = true;
                while (true) {
                    uint64_t min_timestamp = data_period.stop;
                    size_t index_min = ticks.size() + 1;
                    for (size_t i = 0; i < ticks.size(); ++i) {
                        if (ticks[i].timestamp != 0 &&
                            min_timestamp >= ticks[i].timestamp) {
                            min_timestamp = ticks[i].timestamp;
                            index_min = i;
                            is_exit2 = false;
                        }
                    }

                    if (index_min == (ticks.size() + 1)) break;
                    if (ticks[index_min].timestamp > min_next_timestamp) break;
                    callback(index_min, ticks[index_min]);
                    ticks[index_min] = Tick();
                }

                if (is_exit && is_exit2) break;
            }
		}

		enum TypesPrice {
            USE_BID = 0,        /**< Использовать цену bid */
            USE_ASK = 1,        /**< Использовать цену ask */
            USE_AVERAGE = 2,    /**< Использовать среднюю цену */
		};

		enum TypesDirections {
            DIR_ERROR = -2,
            DIR_UNDEFINED = 0,
            DIR_UP = 1,
            DIR_DOWN = -1
		};

        /** \brief Получить направление цены (имитация бинарного опциона)
         *
         * \param type          Тип цены
         * \param timestamp_ms  Время, от которого начинаем проверку направления
         * \param expiration_ms Экспирация
         * \param delay_ms      Задержка
         * \param period_ms     Период дискретизации. Указать 0, если замер происходит в любую миллисекунду
         * \return Вернет направление цены или ошибку
         */
		inline TypesDirections get_direction(
                const TypesPrice type,
                const uint64_t timestamp_ms,
                const uint64_t expiration_ms,
                const uint64_t delay_ms,
                const uint64_t period_ms = 0) {
            Tick tick_1 = get_first_lower(timestamp_ms);
            if (tick_1.empty()) return TypesDirections::DIR_ERROR;
            const uint64_t t1_delay = tick_1.server_timestamp + delay_ms;
            const uint64_t t1 = period_ms == 0 ? t1_delay : (t1_delay - (t1_delay % period_ms) + period_ms);
            const uint64_t t2 = t1 + expiration_ms;
            Tick tick_2 = get_first_lower(t2, true);
            if (tick_2.empty()) return TypesDirections::DIR_ERROR;
            const double open = type == TypesPrice::USE_BID ? tick_1.bid : (type == TypesPrice::USE_ASK ? tick_1.ask : ((tick_1.bid + tick_1.ask)/2.0d));
            const double close = type == TypesPrice::USE_BID ? tick_2.bid : (type == TypesPrice::USE_ASK ? tick_2.ask : ((tick_2.bid + tick_2.ask)/2.0d));
            if (close > open) return TypesDirections::DIR_UP;
            if (close < open) return TypesDirections::DIR_DOWN;
            return TypesDirections::DIR_UNDEFINED;
		}

		inline void wait() noexcept {
            while (true) {
                {
                    std::lock_guard<std::mutex> lock(write_buffer_mutex);
                    if (!is_writing) return;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(wait_delay));
            }
		}

		void set_symbol(const std::string& symbol) noexcept {
            create_protected_transaction([&, symbol](){
                Note kv{"symbol", symbol};
                storage.replace(kv);
            },[&](const uint64_t error_counter){ TRADING_DB_TICK_DB_PRINT << "trading-db: sqlite transaction error! Line " << __LINE__ << ", counter = " << error_counter << std::endl; });
		}

		std::string get_symbol() noexcept {
            std::string temp;
            create_protected_transaction([&](){
                if (auto kv = storage.get_pointer<Note>("symbol")) {
                    temp = kv->value;
                }
            },[&](const uint64_t error_counter){ TRADING_DB_TICK_DB_PRINT << "trading-db: sqlite transaction error! Line " << __LINE__ << ", counter = " << error_counter << std::endl; });
            return temp;
		}

		void set_digits(const uint32_t digits) noexcept {
            Note kv{"digits", std::to_string(digits)};
            create_protected_transaction([&, kv](){
                storage.replace(kv);
            },[&](const uint64_t error_counter){ TRADING_DB_TICK_DB_PRINT << "trading-db: sqlite transaction error! Line " << __LINE__ << ", counter = " << error_counter << std::endl; });
		}

		uint32_t get_digits() noexcept {
            std::string temp;
            create_protected_transaction([&](){
                if (auto kv = storage.get_pointer<Note>("digits")) {
                    temp = kv->value;
                }
            },[&](const uint64_t error_counter){ TRADING_DB_TICK_DB_PRINT << "trading-db: sqlite transaction error! Line " << __LINE__ << ", counter = " << error_counter << std::endl; });
            return std::stoi(temp);
		}

		void set_server_name(const std::string &server_name) noexcept {
            Note kv{"server_name", server_name};
            create_protected_transaction([&, kv](){
                storage.replace(kv);
            },[&](const uint64_t error_counter){ TRADING_DB_TICK_DB_PRINT << "trading-db: sqlite transaction error! Line " << __LINE__ << ", counter = " << error_counter << std::endl; });
		}

		std::string get_server_name() noexcept {
            std::string temp;
            create_protected_transaction([&](){
                if (auto kv = storage.get_pointer<Note>("server_name")) {
                    temp = kv->value;
                }
            },[&](const uint64_t error_counter){ TRADING_DB_TICK_DB_PRINT << "trading-db: sqlite transaction error! Line " << __LINE__ << ", counter = " << error_counter << std::endl; });
            return temp;
		}

		void set_hostname(const std::string& hostname) noexcept {
            Note kv{"hostname", hostname};
            create_protected_transaction([&](){
                storage.replace(kv);
            },[&](const uint64_t error_counter){ TRADING_DB_TICK_DB_PRINT << "trading-db: sqlite transaction error! Line " << __LINE__ << ", counter = " << error_counter << std::endl; });
		}

		std::string get_hostname() noexcept {
            std::string temp;
            create_protected_transaction([&](){
                if (auto kv = storage.get_pointer<Note>("hostname")) {
                    temp = kv->value;
                }
            },[&](const uint64_t error_counter){ TRADING_DB_TICK_DB_PRINT << "trading-db: sqlite transaction error! Line " << __LINE__ << ", counter = " << error_counter << std::endl; });
            return temp;
		}

		void set_comment(const std::string& comment) noexcept {
            Note kv{"comment", comment};
            create_protected_transaction([&](){
                storage.replace(kv);
            },[&](const uint64_t error_counter){ TRADING_DB_TICK_DB_PRINT << "trading-db: sqlite transaction error! Line " << __LINE__ << ", counter = " << error_counter << std::endl; });
		}

		std::string get_comment() noexcept {
            std::string temp;
            create_protected_transaction([&](){
                if (auto kv = storage.get_pointer<Note>("comment")) {
                    temp = kv->value;
                }
            },[&](const uint64_t error_counter){ TRADING_DB_TICK_DB_PRINT << "trading-db: sqlite transaction error! Line " << __LINE__ << ", counter = " << error_counter << std::endl; });
            return temp;
		}

		void set_note(const std::string& key, const std::string& value) noexcept {
		    Note kv{key, value};
            storage.replace(kv);
            create_protected_transaction([&](){
                storage.replace(kv);
            },[&](const uint64_t error_counter){ TRADING_DB_TICK_DB_PRINT << "trading-db: sqlite transaction error! Line " << __LINE__ << ", counter = " << error_counter << std::endl; });
		}

		std::string get_note(const std::string& key) noexcept {
			std::string temp;
            create_protected_transaction([&, key](){
                if (auto kv = storage.get_pointer<Note>(key)) {
                    temp = kv->value;
                }
            },[&](const uint64_t error_counter){ TRADING_DB_TICK_DB_PRINT << "trading-db: sqlite transaction error! Line " << __LINE__ << ", counter = " << error_counter << std::endl; });
            return temp;
		}

		bool backup(const std::string &backup_file_name) {
            if (is_backup) return false;
            is_backup = true;
            async_tasks.create_task([&, backup_file_name]() {
                auto backup = storage.make_backup_to(backup_file_name);
                create_protected([&, backup_file_name](){
                    //std::lock_guard<std::mutex> lock(backup_mutex);
                    //TRADING_DB_TICK_DB_PRINT << "trading-db: " << db_file_name << " backup " << std::endl;
                    //storage.backup_to(backup_file_name);
                    //
                    TRADING_DB_TICK_DB_PRINT << "trading-db: " << db_file_name << " backup " << std::endl;
                    do {
                        //std::lock_guard<std::mutex> lock(backup_mutex);
                        backup.step(1);
                    } while(backup.remaining() > 0);
                    TRADING_DB_TICK_DB_PRINT << "trading-db: " << db_file_name << " backup end" << std::endl;
                },[&, backup_file_name](const uint64_t error_counter){
                    TRADING_DB_TICK_DB_PRINT << "trading-db: " << db_file_name
                        << " sqlite backup " << backup_file_name
                        << "error! Line " << __LINE__ << ", Counter = "
                        << error_counter << std::endl;
                },wait_process_delay);
                is_backup = false;
            });

            /*
            async_tasks.create_task([&]() {
                create_protected([&](){
                    std::lock_guard<std::mutex> lock(backup_mutex);
                },[&](const uint64_t error_counter){

                },wait_process_delay);
                is_backup = false;
            });
            */
            return true;
		};
	};
};

#endif /* TRADING_TICK_DB_HPP */
