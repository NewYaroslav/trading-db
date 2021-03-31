#pragma once
#ifndef TRADING_DB_TICK_DB_HPP
#define TRADING_DB_TICK_DB_HPP

#include "utility/asyn-tasks.hpp"
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
                return (bid == 0);
            }
        };

        using Period = xtime::period_t;

        /** \brief Структура настроек
         */
        class Note {
        public:
            std::string key;
            std::string value;
        };

    private:
        size_t write_buffer_size = 100; /**< Размер буфера для записи */
        size_t write_buffer_index = 0;

        AsynTasks asyn_tasks;

        std::deque<Tick> write_buffer;
        std::mutex write_buffer_mutex;
        std::deque<Tick> read_buffer;
        std::deque<Period> read_periods;
        std::mutex read_buffer_mutex;
        Period write_period;
        Period read_period;

        std::deque<Period> periods; /**< Периоды, когда тики из БД можно использовать */
        std::mutex periods_mutex;

		uint64_t indent_past = xtime::MILLISECONDS_IN_SECOND * xtime::SECONDS_IN_HOUR;
		uint64_t indent_future = xtime::MILLISECONDS_IN_SECOND * xtime::SECONDS_IN_HOUR;

		std::atomic<bool> is_recording = ATOMIC_VAR_INIT(false);
		std::atomic<bool> is_stop_write = ATOMIC_VAR_INIT(false);
		std::atomic<bool> is_writing = ATOMIC_VAR_INIT(false);
		std::atomic<bool> is_update = ATOMIC_VAR_INIT(false);
		std::atomic<bool> is_shutdown = ATOMIC_VAR_INIT(false);

        /** \brief Получить размер буфера для записи
         */
		inline size_t get_write_buffer_size() noexcept {
            std::lock_guard<std::mutex> lock(write_buffer_mutex);
            return write_buffer.size();
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
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
            }
		}

        /** \brief Найти тик по метке времени
         */
		inline Tick find_tick(
                const std::deque<Tick> &buffer,
                const uint64_t timestamp_ms) noexcept {
            if (buffer.empty()) return Tick();
            auto it = std::lower_bound(
                buffer.begin(),
                buffer.end(),
                timestamp_ms,
                [](const Tick & l, const uint64_t timestamp_ms) {
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
            return Tick();
		}

        /* столбец таблицы */
        template<class O, class T, class ...Op>
        using Column = sqlite_orm::internal::column_t<O, T, const T& (O::*)() const, void (O::*)(T), Op...>;

        /* тип хранилища
         *
         * Внимание! Внимание! Внимание!
         *
         * Компилятор mingw вылетает, когда столбцов в таблице становится от 15 шт.
         */
        using Storage = sqlite_orm::internal::storage_t<
            sqlite_orm::internal::table_t<Tick,
                Column<Tick, decltype(Tick::timestamp), sqlite_orm::constraints::primary_key_t<>>,
                Column<Tick, decltype(Tick::server_timestamp)>,
                Column<Tick, decltype(Tick::bid)>,
                Column<Tick, decltype(Tick::ask)>
            >,
            sqlite_orm::internal::table_t<Period,
                Column<Period, decltype(Period::start), sqlite_orm::constraints::primary_key_t<>>,
                Column<Period, decltype(Period::stop)>
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
                sqlite_orm::make_table("Periods",
                    sqlite_orm::make_column("start", &Period::start, sqlite_orm::primary_key()),
                    sqlite_orm::make_column("stop", &Period::stop)),
                sqlite_orm::make_table("Note",
                    sqlite_orm::make_column("key", &Note::key, sqlite_orm::primary_key()),
                    sqlite_orm::make_column("value", &Note::value)));
		}

        /** \brief Инициализация объектов класса
         */
		inline void init_other() noexcept {
            //storage.open_forever();
            storage.sync_schema();

            /* загружаем все периоды */
            std::deque<Period> temp = storage.get_all<Period, std::deque<Period>>();

            /* сортируем периоды, если нужно */
            xtime::sort_periods(temp);

            /* объединяем пересекающиеся периоды */
            std::deque<Period> insert_periods, remove_periods;
            if (xtime::merge_periods(temp, insert_periods, remove_periods)) {
                /* зафиксируем изменения */
                if (!remove_periods.empty()) {
                    create_protected_transaction([&](){
                        for (const Period& p : remove_periods) {
                            storage.remove<Period>(p.start);
                        }
                    },[&](const uint64_t error_counter){ std::cerr << "trading-db: sqlite transaction error! Label 0x01, Counter = " << error_counter << std::endl; });
                } // if (!remove_periods.empty())
                if (!insert_periods.empty()) {
                    create_protected_transaction([&](){
                        for (const Period& p : insert_periods) {
                            storage.replace(p);
                        }
                    },[&](const uint64_t error_counter){ std::cerr << "trading-db: sqlite transaction error! Label 0x02, Counter = " << error_counter << std::endl; });
                } // if (!insert_periods.empty())
            } // if

            {
                std::lock_guard<std::mutex> lock(periods_mutex);
                periods = std::move(temp);
            }

            /* запускаем асинхронную задачу
             * в данном потоке мы будем делать запись в БД
             */
            asyn_tasks.creat_task([&] {
                while (true) {
                    /* ожидаем заполнения буфера */
                    while (get_write_buffer_size() < write_buffer_size) {
                        if (is_stop_write) break;
                        if (is_shutdown) return;
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }

                    /* запоминаем данные для записи */
                    std::deque<Tick> transaction_buffer;
                    Period transaction_period;
                    {
                        std::lock_guard<std::mutex> lock(write_buffer_mutex);
                        transaction_buffer = write_buffer;
                        transaction_period = write_period;
                    }

                    /* записываем данные в БД */
                    std::deque<Period> transaction_periods;
                    {
                        std::lock_guard<std::mutex> lock(periods_mutex);
                        transaction_periods = periods;
                    }
                    is_writing = true;

                    write_data_to_db(
                        transaction_buffer,
                        transaction_period,
                        transaction_periods,
                            [&](const std::deque<Period> &new_periods){
                        std::lock_guard<std::mutex> lock(periods_mutex);
                        periods = new_periods;
                    });

                    /* удаляем данные из временного буфера */
                    {
                        std::lock_guard<std::mutex> lock(write_buffer_mutex);
                        write_buffer.erase(write_buffer.begin(), write_buffer.begin() + transaction_buffer.size());
                        if (write_buffer.size() == 0) {
                            is_writing = false;
                            if (is_stop_write) {
                                is_recording = false;
                                write_period = Period();
                                is_stop_write = false;
                            }
                        }
                        is_update = true;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            });
		}

        /** \brief Записать данные в БД
         */
		inline void write_data_to_db(
                const std::deque<Tick> &tick_buffer,
                const Period tick_period,
                std::deque<Period> &all_periods,
                const std::function<void(const std::deque<Period> &new_periods)> callback_periods) {
            const int NOT_WRITE_PERIOD = 0;
            const int ADD_PERIOD = 1;
            const int REPLACE_PERIOD = 2;
            const int REPLACE_ALL_PERIODS_1 = 3;
            const int REPLACE_ALL_PERIODS_2 = 4;
            int write_period_status = NOT_WRITE_PERIOD;

            std::deque<Period> insert_periods, remove_periods;

            if (all_periods.empty()) {
                write_period_status = ADD_PERIOD;
                all_periods.push_back(tick_period);
            } else {
                /* проверяем распространенные случаи */
                if (all_periods.back().stop < tick_period.start) {
                    write_period_status = ADD_PERIOD;
                    all_periods.push_back(tick_period);
                } else
                if (all_periods.back().start == tick_period.start &&
                    all_periods.back().stop < tick_period.stop) {
                    write_period_status = REPLACE_PERIOD;
                    all_periods.back() = tick_period;
                } else
                if (all_periods.back().start == tick_period.start &&
                    all_periods.back().stop == tick_period.stop) {
                    /* ничего не делаем */
                } else {
                    /* у нас сложный пациент, решаем сложным путем */
                    auto it_start = xtime::find_period(all_periods, tick_period.start);
                    if (it_start == all_periods.end()) {
                        /* период не найден, значит сначала его запишем */
                        write_period_status = REPLACE_ALL_PERIODS_1;
                        all_periods.push_back(tick_period);
                    } else {
                        /* период найден, значит его записывать не надо */
                        write_period_status = REPLACE_ALL_PERIODS_2;
                        it_start->stop = std::max(tick_period.stop, it_start->stop);
                    }
                    xtime::sort_periods(all_periods);
                    xtime::merge_periods(all_periods, insert_periods, remove_periods);
                }
            } // if (all_periods.empty())

            if (!tick_buffer.empty()) {
                create_protected_transaction([&](){
                    for (const Tick& t : tick_buffer) {
                        storage.replace(t);
                    }
                },[&](const uint64_t error_counter){ std::cerr << "trading-db: sqlite transaction error! Label 0x03, Counter = " << error_counter << std::endl; });

                // std::cout << "storage.transaction tick_buffer size " << tick_buffer.size() << std::endl;
            }

            switch(write_period_status) {
            case ADD_PERIOD:
            case REPLACE_PERIOD:
                create_protected_transaction([&](){
                    storage.replace(tick_period);
                },[&](const uint64_t error_counter){ std::cerr << "trading-db: sqlite transaction error! Label 0x04, Counter = " << error_counter << std::endl; });
                break;
            case REPLACE_ALL_PERIODS_1:
                storage.transaction([&]() mutable {
                    storage.replace(tick_period);
                    return true;
                });
            case REPLACE_ALL_PERIODS_2:
                if (!remove_periods.empty()) {
                    storage.transaction([&]() mutable {
                        for (const Period p : remove_periods) {
                            storage.remove<Period>(p.start);
                        }
                        return true;
                    });
                }
                if (!insert_periods.empty()) {
                    storage.transaction([&]() mutable {
                        for (const Period& p : insert_periods) {
                            storage.replace(p);
                        }
                        return true;
                    });
                }
                break;
            };

            switch(write_period_status) {
            case ADD_PERIOD:
            case REPLACE_PERIOD:
            case REPLACE_ALL_PERIODS_1:
            case REPLACE_ALL_PERIODS_2:
                callback_periods(all_periods);
                break;
            default:
                break;
            };
		}

        /** \brief Прочитать данные тиков на заданном промежутке времени из БД в буфер
         */
		void read_db_to_buffer(
                std::deque<Tick> &buffer,
                Period &period,
                const uint64_t timestamp_ms) noexcept {
            using namespace sqlite_orm;
            period.start = timestamp_ms > indent_past ? timestamp_ms - indent_past : 0;
            period.stop = timestamp_ms + indent_future;
            if (!buffer.empty()) buffer.clear();
            std::deque<Tick> beg_element, end_element;

            create_protected_transaction([&](){
                beg_element = storage.get_all<Tick, std::deque<Tick>>(
                    where(c(&Tick::timestamp) <= period.start),
                    order_by(&Tick::timestamp).desc(),
                    limit(1));
            },[&](const uint64_t error_counter){ std::cerr << "trading-db: sqlite transaction error! Label 0x08, Counter = " << error_counter << std::endl; });

            create_protected_transaction([&](){
                buffer = storage.get_all<Tick, std::deque<Tick>>(where(
                        c(&Tick::timestamp) < period.stop &&
                        c(&Tick::timestamp) > period.start));
            },[&](const uint64_t error_counter){ std::cerr << "trading-db: sqlite transaction error! Label 0x09, Counter = " << error_counter << std::endl; });

            create_protected_transaction([&](){
                end_element = storage.get_all<Tick, std::deque<Tick>>(
                    where(c(&Tick::timestamp) >= period.stop),
                    order_by(&Tick::timestamp).asc(),
                    limit(1));
            },[&](const uint64_t error_counter){ std::cerr << "trading-db: sqlite transaction error! Label 0x0A, Counter = " << error_counter << std::endl; });

            if (!beg_element.empty()) buffer.push_front(beg_element.back());
            if (!end_element.empty()) buffer.push_front(end_element.front());
		}

		/** \brief Прочитать данные периодов за заданный промежуток времени из БД в буфер
         */
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

	public:

        /** \brief Конструктор хранилища тиков
         * \param path Путь к файлу
         */
		TickDb(const std::string &path) : storage(init_storage(path)) {
            init_other();
		}

		~TickDb() {
            if (is_recording && get_write_buffer_size() != 0) {
                uint64_t timestamp = 0;
                {
                    std::lock_guard<std::mutex> lock(write_buffer_mutex);
                    timestamp = write_buffer.back().timestamp;
                }
                stop_write(timestamp);
            }
            /* ждем записи всех данных */
            wait();
            is_shutdown = true;
		}

        /** \brief Записать новый тик
         * \param new_tick  Новый тик
         * \return Вернет true в случае успешной записи
         */
        bool write(const Tick& new_tick) noexcept {
            if (is_stop_write) return false;
            if (!is_recording) {
                /* если запись еще не начата,
                 * инициализируем период записи
                 */
                std::lock_guard<std::mutex> lock(write_buffer_mutex);
                write_period.start = new_tick.timestamp;
                write_period.stop = new_tick.timestamp;
                write_buffer.push_back(new_tick);
                is_recording = true;
                return true;
            }
            {
                std::lock_guard<std::mutex> lock(write_buffer_mutex);
                if (new_tick.timestamp > write_buffer.back().timestamp) {
                    write_buffer.push_back(new_tick);
                    write_period.stop = new_tick.timestamp;
                } else {
                    auto it = std::lower_bound(
                        write_buffer.begin(),
                        write_buffer.end(),
                        new_tick,
                        [](const Tick & l, const Tick & r) {
                            return l.timestamp < r.timestamp;
                        });

                    write_buffer.insert(it, new_tick);
                    write_period.start = std::min(write_period.start, new_tick.timestamp);
                    write_period.stop = std::max(write_period.stop, new_tick.timestamp);
                }
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
         * \param timestamp_ms Метка времени остановки записи тиков
         */
		bool stop_write(const uint64_t timestamp_ms) noexcept {
            if (!is_recording) return false;
            if (is_stop_write) return false;
            {
                std::lock_guard<std::mutex> lock(write_buffer_mutex);
                write_period.start = std::min(write_period.start, timestamp_ms);
                write_period.stop = std::max(write_period.stop, timestamp_ms);
            }
            is_stop_write = true;
            is_writing = true;
            return true;
		}

		Tick get(const uint64_t timestamp_ms, const bool db_only = false) noexcept {
            if (!db_only && is_recording) {
                /* ищем данные в буфере */
                std::lock_guard<std::mutex> lock(write_buffer_mutex);
                if (!write_buffer.empty() &&
                    ((!is_stop_write && timestamp_ms >= write_period.start) ||
                    (is_stop_write && timestamp_ms >= write_period.start &&
                    timestamp_ms <= write_period.stop))) {
                    Tick tick = find_tick(write_buffer, timestamp_ms);
                    if (!tick.empty()) return tick;
                }
            } // if (!db_only)

            {
                std::lock_guard<std::mutex> lock(periods_mutex);
                auto it_period = xtime::find_period(periods, timestamp_ms);
                if (it_period == periods.end()) {
                    return Tick();
                } else {
                    // std::cout << "get 0.1 " << it_period->start << " " << it_period->stop << std::endl;
                    if (it_period->start == timestamp_ms ||
                        it_period->stop == timestamp_ms) {
                        /* если мы находимся на границе данных, перезагрузим данные */
                        std::lock_guard<std::mutex> lock(read_buffer_mutex);
                        read_db_to_buffer(read_buffer, read_period, timestamp_ms);
                        return find_tick(read_buffer, timestamp_ms);
                    } else
                    if (is_update) {
                        /* если есть обновление данных в БД, загрузим данные */
                        std::lock_guard<std::mutex> lock(read_buffer_mutex);
                        read_db_to_buffer(read_buffer, read_period, timestamp_ms);
                        is_update = false;
                        return find_tick(read_buffer, timestamp_ms);
                    } else {
                        std::lock_guard<std::mutex> lock(read_buffer_mutex);
                        if (read_buffer.empty() ||
                            read_period.start >= timestamp_ms ||
                            read_period.stop <= timestamp_ms) {
                            read_db_to_buffer(read_buffer, read_period, timestamp_ms);
                        }
                        return find_tick(read_buffer, timestamp_ms);
                    }
                }
            }
            return Tick();
		}

		inline void wait() noexcept {
            while (is_writing) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
		}

		void set_symbol(const std::string& symbol) noexcept {
            Note kv{"symbol", symbol};
            create_protected_transaction([&, kv](){
                storage.replace(kv);
            },[&](const uint64_t error_counter){ std::cerr << "trading-db: sqlite sqlite transaction error! Label 0x0С, Counter = " << error_counter << std::endl; });
		}

		std::string get_symbol() noexcept {
            std::string temp;
            create_protected_transaction([&](){
                if (auto kv = storage.get_pointer<Note>("symbol")) {
                    temp = kv->value;
                }
            },[&](const uint64_t error_counter){ std::cerr << "trading-db: sqlite sqlite transaction error! Label 0x0D, Counter = " << error_counter << std::endl; });
            return temp;
		}

		void set_digits(const uint32_t digits) noexcept {
            Note kv{"digits", std::to_string(digits)};
            create_protected_transaction([&, kv](){
                storage.replace(kv);
            },[&](const uint64_t error_counter){ std::cerr << "trading-db: sqlite sqlite transaction error! Label 0x0E, Counter = " << error_counter << std::endl; });
		}

		uint32_t get_digits() noexcept {
            std::string temp;
            create_protected_transaction([&](){
                if (auto kv = storage.get_pointer<Note>("digits")) {
                    temp = kv->value;
                }
            },[&](const uint64_t error_counter){ std::cerr << "trading-db: sqlite sqlite transaction error! Label 0x0F, Counter = " << error_counter << std::endl; });
            return std::stoi(temp);
		}

		void set_server_name(const std::string &server_name) noexcept {
            Note kv{"server_name", server_name};
            create_protected_transaction([&, kv](){
                storage.replace(kv);
            },[&](const uint64_t error_counter){ std::cerr << "trading-db: sqlite sqlite transaction error! Label 0x10, Counter = " << error_counter << std::endl; });
		}

		std::string get_server_name() noexcept {
            std::string temp;
            create_protected_transaction([&](){
                if (auto kv = storage.get_pointer<Note>("server_name")) {
                    temp = kv->value;
                }
            },[&](const uint64_t error_counter){ std::cerr << "trading-db: sqlite sqlite transaction error! Label 0x11, Counter = " << error_counter << std::endl; });
            return temp;
		}

		void set_hostname(const std::string& hostname) noexcept {
            Note kv{"hostname", hostname};
            create_protected_transaction([&](){
                storage.replace(kv);
            },[&](const uint64_t error_counter){ std::cerr << "trading-db: sqlite sqlite transaction error! Label 0x12, Counter = " << error_counter << std::endl; });
		}

		std::string get_hostname() noexcept {
            std::string temp;
            create_protected_transaction([&](){
                if (auto kv = storage.get_pointer<Note>("hostname")) {
                    temp = kv->value;
                }
            },[&](const uint64_t error_counter){ std::cerr << "trading-db: sqlite sqlite transaction error! Label 0x13, Counter = " << error_counter << std::endl; });
            return temp;
		}

		void set_comment(const std::string& comment) noexcept {
            Note kv{"comment", comment};
            create_protected_transaction([&](){
                storage.replace(kv);
            },[&](const uint64_t error_counter){ std::cerr << "trading-db: sqlite sqlite transaction error! Label 0x14, Counter = " << error_counter << std::endl; });
		}

		std::string get_comment() noexcept {
            std::string temp;
            create_protected_transaction([&](){
                if (auto kv = storage.get_pointer<Note>("comment")) {
                    temp = kv->value;
                }
            },[&](const uint64_t error_counter){ std::cerr << "trading-db: sqlite sqlite transaction error! Label 0x15, Counter = " << error_counter << std::endl; });
            return temp;
		}

		void set_note(const std::string& key, const std::string& value) noexcept {
		    Note kv{key, value};
            storage.replace(kv);
            create_protected_transaction([&](){
                storage.replace(kv);
            },[&](const uint64_t error_counter){ std::cerr << "trading-db: sqlite sqlite transaction error! Label 0x16, Counter = " << error_counter << std::endl; });
		}

		std::string get_note(const std::string& key) noexcept {
			std::string temp;
            create_protected_transaction([&, key](){
                if (auto kv = storage.get_pointer<Note>(key)) {
                    temp = kv->value;
                }
            },[&](const uint64_t error_counter){ std::cerr << "trading-db: sqlite sqlite transaction error! Label 0x17, Counter = " << error_counter << std::endl; });
            return temp;
		}
	};

};

#endif /* TRADING_TICK_DB_HPP */
