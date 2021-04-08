#pragma once
#ifndef TRADING_DB_TICK_DB_HPP
#define TRADING_DB_TICK_DB_HPP

#if SQLITE_THREADSAFE != 2
#error "The project must be built for sqlite multithreading! Set the SQLITE_THREADSAFE=2"
#endif

#include "utility/asyn-tasks.hpp"
#include "utility/print.hpp"
#include <sqlite_orm/sqlite_orm.h>
#include <xtime.hpp>
#include <mutex>
#include <atomic>
#include <future>

#define TRADING_DB_TICK_DB_PRINT PrintThread{}

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
                return (bid == 0);
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

        size_t wait_delay = 10;
        size_t write_buffer_size = 100; /**< Размер буфера для записи */
        size_t write_buffer_index = 0;

        AsynTasks asyn_tasks;

        std::deque<Tick> write_buffer;
        EndTickStamp write_end_tick_stamp;
        std::mutex write_buffer_mutex;
        std::atomic<uint32_t> write_buffer_restart_size = ATOMIC_VAR_INIT(0);

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

        std::atomic<uint32_t> wait_stop_time = ATOMIC_VAR_INIT(60);
        std::atomic<uint32_t> wait_stop_timestamp = ATOMIC_VAR_INIT(0);

        /** \brief Получить размер буфера для записи
         */
		inline size_t get_write_buffer_size() noexcept {
            std::lock_guard<std::mutex> lock(write_buffer_mutex);
            return write_buffer.size();
		}

		inline void create_protected(
                const std::function<void()> &callback,
                const std::function<void(const uint64_t counter)> &callback_error,
                const uint64_t delay = 20) {
            uint64_t counter = 0;
            while (!is_shutdown) {
                try {
                    callback();
                    break;
                } catch(...) {
                    ++counter;
                    if (callback_error != nullptr) callback_error(counter);
                    std::this_thread::sleep_for(std::chrono::milliseconds(wait_delay));
                    continue;
                }
            }
		}

		inline void create_protected_transaction(
                const std::function<void()> &callback,
                const std::function<void(const uint64_t counter)> &callback_error,
                const uint64_t delay = 20) {
            uint64_t counter = 0;
            while (!is_shutdown) {
                try {
                    storage.transaction([&, callback]() mutable {
                        callback();
                        return true;
                    });
                    break;
                } catch(...) {
                    ++counter;
                    if (callback_error != nullptr) callback_error(counter);
                    std::this_thread::sleep_for(std::chrono::milliseconds(wait_delay));
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

        /** \brief Инициализация объектов класса
         */
		inline void init_other() noexcept {
            /* если вы хотите получить доступ к хранилищу из разных потоков,
             * вы должны вызвать его open_foreverсразу после создания хранилища,
             * потому что хранилище может создавать разные соединения
             * в результате гонок данных внутри хранилища
             * https://github.com/fnc12/sqlite_orm/issues/163
             */
            storage.open_forever();
            storage.sync_schema();
            storage.busy_timeout(1000);

            /* запускаем асинхронную задачу
             * в данном потоке мы будем делать запись в БД
             */
            asyn_tasks.creat_task([&]() {
                uint64_t stop_timestamp = 0;
                bool is_stop_timestamp = false;
                while (true) {
                    bool is_write_buffer = false;
                    bool is_write_end_tick_stamp = false;

                    /* запоминаем данные для записи */
                    std::deque<Tick> transaction_buffer;
                    {
                        std::lock_guard<std::mutex> lock(write_buffer_mutex);
                        if (xtime::get_timestamp() > wait_stop_timestamp) is_stop_write = true;
                        if (is_restart_write) {
                            /* если был рестарт, копируем только часть данных */
                            if (write_buffer_restart_size > 0) {
                                transaction_buffer.resize(write_buffer_restart_size);
                                transaction_buffer.assign(write_buffer.begin(), write_buffer.begin() + write_buffer_restart_size);

                                stop_timestamp = transaction_buffer.back().timestamp;
                                write_end_tick_stamp.timestamp = stop_timestamp;
                                is_stop_timestamp = true;

                                is_write_buffer = true;
                                is_write_end_tick_stamp = true;
                            }
                            is_restart_write = false;
                        } else
                        if (write_buffer.size() > write_buffer_size || is_stop_write) {
                            if (!write_buffer.empty()) {
                                transaction_buffer.resize(write_buffer.size());
                                transaction_buffer.assign(write_buffer.begin(), write_buffer.end());

                                stop_timestamp = transaction_buffer.back().timestamp;
                                write_end_tick_stamp.timestamp = stop_timestamp;
                                is_stop_timestamp = true;

                                is_write_buffer = true;
                            }
                            if (is_stop_write) {
                                if (is_stop_timestamp) {
                                    is_write_end_tick_stamp = true;
                                }
                                is_recording = false;
                            }
                            is_stop_write = false;
                        }
                    }

                    /* записываем данные в БД */
                    if (is_write_buffer) {
                        std::cout << db_file_name << " ---4.1" << std::endl;
                        create_protected_transaction([&, transaction_buffer](){
                            for (const Tick& t : transaction_buffer) {
                                storage.replace(t);
                            }
                        },[&](const uint64_t error_counter){
                            TRADING_DB_TICK_DB_PRINT
                                << "trading-db: " << db_file_name
                                << " sqlite transaction error! Line " << __LINE__ << ", counter = "
                                << error_counter << std::endl;
                        });
                    }
                    if (is_write_end_tick_stamp) {
                        create_protected_transaction([&, stop_timestamp](){
                            EndTickStamp end_tick_stamp(stop_timestamp);
                            storage.replace(end_tick_stamp);
                        },[&](const uint64_t error_counter){
                            TRADING_DB_TICK_DB_PRINT
                                << "trading-db: " << db_file_name
                                << " sqlite transaction error! Line " << __LINE__ << ", counter = "
                                << error_counter << std::endl;
                        });
                        is_stop_timestamp = false;
                    }

                    if (is_write_buffer) {
                        std::lock_guard<std::mutex> lock(write_buffer_mutex);
                        /* очищаем буфер с котировками */
                        if (!write_buffer.empty() && !transaction_buffer.empty()) {
                            write_buffer.erase(write_buffer.begin(), write_buffer.begin() + transaction_buffer.size());
                        }
                    }

                    {
                        std::lock_guard<std::mutex> lock(write_buffer_mutex);
                        if (is_stop_timestamp || !write_buffer.empty()) {
                            is_writing = true;
                        } else {
                            is_writing = false;
                            if (is_shutdown) break;
                        }
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(wait_delay));
                }
                is_writing = false;
                is_stop = true;
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
                });

                create_protected([&](){
                    end_element = storage.get_all<Tick, std::deque<Tick>>(
                        where(c(&Tick::timestamp) >= period.stop),
                        order_by(&Tick::timestamp).asc(),
                        limit(1));
                },[&](const uint64_t error_counter){
                    TRADING_DB_TICK_DB_PRINT << "trading-db: " << db_file_name
                        << " sqlite get_all error! Line " << __LINE__ << ", counter = "
                        << error_counter << std::endl;
                });

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
            /* ждем завершения бэкапа */
            while(is_backup) {
                std::this_thread::sleep_for(std::chrono::milliseconds(wait_delay));
            }

            is_block_write = true;
            /* ставим флаг остановки записи */
            {
                std::lock_guard<std::mutex> lock(write_buffer_mutex);
                is_stop_write = true;
            }
            wait();
            is_shutdown = true;
            /* ждем завершение потоков */
            while(!is_stop) {
                std::this_thread::sleep_for(std::chrono::milliseconds(wait_delay));
            }
            asyn_tasks.clear();
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
            } else
            if (!is_restart_write) {
                /* останавливаем запись и возобновляем вновь */
                write_buffer_restart_size = write_buffer.size();
                write_end_tick_stamp.timestamp = write_buffer.back().timestamp;
                write_buffer.push_back(new_tick);
                is_restart_write = true;
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
            write_end_tick_stamp.timestamp = write_buffer.back().timestamp;
            return true;
		}

		inline void set_wait_stop_time(const uint32_t wait_time) {
            wait_stop_time = wait_time;
		}

		inline void reset_stop_counter() {
            const uint64_t temp = wait_stop_time + xtime::get_timestamp();
            wait_stop_timestamp = temp;
		}

		Tick get(const uint64_t timestamp_ms, const bool db_only = false) noexcept {
            if (!db_only) {
                /* ищем данные в буфере */
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

            /* далее идет работа с базой данных, ведь буфер не содержит искомых данных */
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
            Note kv{"symbol", symbol};
            create_protected_transaction([&, kv](){
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
            asyn_tasks.creat_task([&, backup_file_name]() {
                create_protected([&, backup_file_name](){
                    storage.backup_to(backup_file_name);
                },[&, backup_file_name](const uint64_t error_counter){
                    TRADING_DB_TICK_DB_PRINT << "trading-db: " << db_file_name
                        << " sqlite backup " << backup_file_name
                        << "error! Line " << __LINE__ << ", Counter = "
                        << error_counter << std::endl;
                });
                is_backup = false;
            });
            return true;
		};
	};

};

#endif /* TRADING_TICK_DB_HPP */
