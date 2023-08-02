#pragma once
#ifndef TRADING_DB_QDB_FX_HISTORY_HPP_INCLUDED
#define TRADING_DB_QDB_FX_HISTORY_HPP_INCLUDED

#include "../../parts/qdb/enums.hpp"
#include "../../parts/qdb/data-classes.hpp"
#include "../../qdb.hpp"
#include "../../utils/async-tasks.hpp"
#include "fx-symbols-db.hpp"
#include <ztime.hpp>
#include <vector>
#include <set>

namespace trading_db {

    /** \brief Симулятор
     */
    class QdbFxHistoryV1 {
    public:

        using SymbolConfig  = QdbFxSymbolDB::SymbolConfig;
        using TradeFxSignal = QdbFxSymbolDB::TradeFxSignal;
        using TradeFxResult = QdbFxSymbolDB::TradeFxResult;

        /** \brief Конфигурация тестера
         */
        class Config : public QdbFxSymbolDB::Config {
        public:
            uint64_t                    pre_start_period    = 7*ztime::SEC_PER_DAY; /**< Период перед началом теста (UTC, в секундах) */
            uint64_t                    start_date          = 0;        /**< Начальная дата теста (UTC, в секундах) */
            uint64_t                    stop_date           = 0;        /**< Конечная дата теста (UTC, в секундах) */
            double                      tick_period         = 1.0;      /**< Период тиков внутри бара (в секундах) */
            uint64_t                    timeframe           = 60;       /**< Таймфрейм исторических данных (в секундах) */
            bool                        use_new_tick_mode   = false;    /**< Режим "новый тик" разрешает событие on_test только при наступлении нового тика */

            std::vector<TimePeriod>     trade_period;                   /**< Периоды торговли */

            /** \brief Установить даты симулятора
             * Чтобы указать начальную дату, установите переменную stop = false
             * Для конечной даты установить переменную stop = true
             */
            inline void set_date(
                    const bool  stop,
                    const int   day,
                    const int   month,
                    const int   year,
                    const int   hour      = 0,
                    const int   minute    = 0,
                    const int   second    = 0) noexcept {
                if (stop) {
                    stop_date = ztime::get_timestamp(day, month, year, hour, minute, second);
                } else {
                    start_date = ztime::get_timestamp(day, month, year, hour, minute, second);
                }
            };

            inline void add_trade_period(const TimePeriod &user_period) noexcept {
                trade_period.push_back(user_period);
            }

            inline void add_trade_period(
                    const TimePoint &user_start,
                    const TimePoint &user_stop,
                    const int32_t user_id = QDB_TIME_PERIOD_NO_ID) noexcept {
                add_trade_period(TimePeriod(user_start, user_stop, user_id));
            }

            std::function<void(
                    const size_t s_index)>      on_end_test_symbol   = nullptr;

            std::function<void(
                    const size_t i,
                    const size_t n)>            on_end_test_thread   = nullptr;

            std::function<void()>               on_end_test         = nullptr;

            std::function<void(
                    const size_t s_index,
                    const uint64_t t_ms)>       on_date_msg = nullptr;

            std::function<bool(
                    const size_t s_index)>      on_symbol   = nullptr;

            std::function<void(
                    const size_t                s_index,    // Номер символа
                    const uint64_t              t_ms,       // Время тестера
                    const std::set<int32_t>     &period_id, // Флаг периода теста (0 - если нет периода)
                    const trading_db::Candle    &candle     // Данные бара
                    )>                          on_candle    = nullptr;

            std::function<void(
                    const size_t                s_index,    // Номер символа
                    const uint64_t              t_ms,       // Время тестера
                    const std::set<int32_t>     &period_id, // Флаг периода теста (0 - если нет периода)
                    const trading_db::Tick      &tick       // Данные тика
                    )>                          on_tick    = nullptr;

            std::function<void(
                    const size_t                s_index,    // Номер символа
                    const uint64_t              t_ms,       // Время тестера
                    const std::set<int32_t>     &period_id  // Флаг периода теста (0 - если нет периода)
                    )>                          on_test    = nullptr;
        }; // Config

    private:

        class InternalConfig {
        public:
            std::vector<uint64_t>               time_step_ms;
            std::vector<bool>                   candle_flag;
            std::vector<std::set<int32_t>>      period_id;
            QDB_TIMEFRAMES                      timeframe   = QDB_TIMEFRAMES::PERIOD_M1;
            std::map<std::string, size_t>       symbol_to_index;
            std::map<std::thread::id, size_t>   thread_id_to_index;
            std::mutex                          thread_id_mutex;

            void add_thread_index(const size_t index) {
                std::lock_guard<std::mutex> guard(thread_id_mutex);
                thread_id_to_index[std::this_thread::get_id()] = index;
            }

            bool get_thread_index(size_t &index) {
                std::lock_guard<std::mutex> guard(thread_id_mutex);
                auto it = thread_id_to_index.find(std::this_thread::get_id());
                if (it == thread_id_to_index.end()) return false;
                index = it->second;
                return true;
            }

        } m_internal_config;

        std::vector<std::shared_ptr<QdbFxSymbolDB>> m_symbol_db;
        utils::AsyncTasks                           m_async_tasks;

        Config      m_config;

        // Инициализация базы данных
        inline bool init_db() {
            m_symbol_db.clear();
            const size_t number_threads = std::thread::hardware_concurrency();
            for (size_t s = 0; s < number_threads; ++s) {
                m_symbol_db.push_back(std::make_shared<QdbFxSymbolDB>());
                m_symbol_db[s]->set_config(m_config);
                if (!m_symbol_db[s]->init()) return false;
            }
            for (size_t s = 0; s < m_config.symbols.size(); ++s) {
                m_internal_config.symbol_to_index[m_config.symbols[s].symbol] = s;
            }
            return true;
        }

        bool init_trade() {
            // Период получения тиков
            const uint64_t tick_period_ms = (uint64_t)(m_config.tick_period * (double)ztime::MS_PER_SEC + 0.5);
            // Таймфрейм баров
            const uint64_t timeframe_ms = m_config.timeframe * (uint64_t)ztime::MS_PER_SEC;
            m_internal_config.timeframe = static_cast<QDB_TIMEFRAMES>(m_config.timeframe / ztime::SEC_PER_MIN);

            // Настариваем фильтр времени
            for (uint64_t t_ms = 0; t_ms < ztime::MS_PER_DAY; t_ms += tick_period_ms) {
                const uint64_t t = ztime::ms_to_sec(t_ms);
                std::set<int32_t> period_id;
                bool is_trade_period = false;
                for (size_t j = 0; j < m_config.trade_period.size(); ++j) {
                    const auto &p = m_config.trade_period[j];
                    if (p.check_time(t)) {
                        is_trade_period = true;
                        period_id.insert(p.id);
                    }
                }

                const bool is_candle = ((t_ms % timeframe_ms) == 0);
                if (is_trade_period || is_candle) {
                    m_internal_config.time_step_ms.push_back(t_ms);
                    m_internal_config.period_id.push_back(period_id);
                    m_internal_config.candle_flag.push_back(is_candle);
                }
            }
            return true;
        }

    public:

        QdbFxHistoryV1() {};
        ~QdbFxHistoryV1() {};

        inline Config get_config() noexcept {
            return m_config;
        }

        inline void set_config(const Config &arg_config) noexcept {
            m_config = arg_config;
        }

        inline bool init() noexcept {
            if (!init_db()) return false;
            if (!init_trade()) return false;
            return true;
        }

        /** \brief Получить индекс рабочего потока
         * \param index Индекс рабочего потока (находится в диапазоне от 0 до (get_thread_сount() - 1))
         * \return Вернет true в случае успеха
         */
        inline bool get_thread_index(size_t &index) {
            return m_internal_config.get_thread_index(index);
        }

        /** \brief Получить количество рабочих потоков
         * \return Вернет количество рабочих потоков. Индекс рабочего потока будет находиться в диапазоне от 0 до (get_thread_сount() - 1)
         */
        inline size_t get_thread_count() {
            return std::thread::hardware_concurrency();
        }

        /** \brief Посчитать профит
         * \param signal Параметры сигнала
         * \param result Параметры результата
         * \return Вернет true в случае успеха
         */
        inline bool calc_trade_result(
                const TradeFxSignal &signal,
                TradeFxResult       &result) {
            size_t thread_index = 0;
            if (!m_internal_config.get_thread_index(thread_index)) return false;
            return m_symbol_db[thread_index]->calc_trade_result(signal, result);
        }

        /** \brief Запустить тест на истории
         */
        void start() {
            // Количество потоков
            const size_t number_threads = std::thread::hardware_concurrency();
            // Блокировщики доступа к callback-функциям
            std::mutex on_date_mutex;
            std::mutex f_mutex;

            for (size_t n = 0; n < number_threads; ++n) {
                m_async_tasks.create_task([this, &on_date_mutex, &f_mutex, n, number_threads]() {
                    m_internal_config.add_thread_index(n);

                    // Получаем даты проведения теста
                    const uint64_t start_date_ms =
                        ztime::get_first_timestamp_day(m_config.start_date) *
                        ztime::MS_PER_SEC;
                    const uint64_t pre_start_date_ms =
                        ztime::get_first_timestamp_day(m_config.start_date - m_config.pre_start_period) *
                        ztime::MS_PER_SEC;
                    const uint64_t stop_date_ms =
                        ztime::get_first_timestamp_day(m_config.stop_date) *
                        ztime::MS_PER_SEC;

                    //{ Проходимся по всем символам
                    for (size_t s = n; s < m_config.symbols.size(); s += number_threads) {
                        // проверяем необходимость обработать символ
                        if (m_config.on_symbol) {
                            if (!m_config.on_symbol(s)) continue;
                        }

                        // Последнее время обновления индикаторов
                        uint64_t    last_update_time_ms = 0;
                        // Наличие нового тика
                        bool        is_new_tick         = false;

                        const uint64_t tick_period_ms = (uint64_t)(m_config.tick_period * (double)ztime::MS_PER_SEC + 0.5);
                        const uint64_t date_step_ms = ztime::MS_PER_DAY;

                        // Цикл по дате
                        for (uint64_t date_ms = pre_start_date_ms;
                            date_ms <= stop_date_ms;
                            date_ms += date_step_ms) {

                            //{ Выводим сообщение о дате
                            if (m_config.on_date_msg) {
                                std::lock_guard<std::mutex> locker(on_date_mutex);
                                m_config.on_date_msg(s, date_ms);
                            }
                            //} Выводим сообщение о дате

                            // цикл по времени внутри дня
                            for (size_t i = 0; i < m_internal_config.time_step_ms.size(); ++i) {
                                // время внутри дня
                                const uint64_t t_ms = date_ms + m_internal_config.time_step_ms[i];

                                if (m_internal_config.candle_flag[i]) {
                                    //{ Вызываем on_candle
                                    const uint64_t t = ztime::ms_to_sec(t_ms);
                                    const uint64_t timestamp_minute = ztime::get_first_timestamp_minute(t);
                                    const uint64_t timestamp_candle = timestamp_minute - ztime::SEC_PER_MIN;
                                    trading_db::Candle db_candle;
                                    if (m_symbol_db[n]->get_candle(db_candle, s, timestamp_candle, m_internal_config.timeframe)) {
                                        m_config.on_candle(s, t_ms, m_internal_config.period_id[i], db_candle);
                                        last_update_time_ms = (db_candle.timestamp + ztime::SEC_PER_MIN) * ztime::MS_PER_SEC;
                                    }
                                    // для режима вызова on_test по новому тику
                                    if (m_config.use_new_tick_mode) {
                                        trading_db::Tick db_tick;
                                        if (m_symbol_db[n]->get_tick_ms(db_tick, s, t_ms)) {
                                            const uint64_t prev_t_ms = t_ms - tick_period_ms;
                                            if (db_tick.t_ms > prev_t_ms) {
                                                is_new_tick = true;
                                            }
                                        }
                                    }
                                    //} Вызываем on_candle
                                } else {
                                    //{ Вызываем on_tick
                                    trading_db::Tick db_tick;
                                    if (m_symbol_db[n]->get_tick_ms(db_tick, s, t_ms)) {
                                        //{ Проверяем, что пришел новый тик нового бара
                                        if (db_tick.t_ms > last_update_time_ms) {
                                            last_update_time_ms = db_tick.t_ms;
                                            m_config.on_tick(s, t_ms, m_internal_config.period_id[i], db_tick);
                                            is_new_tick = true;
                                        }
                                        //} Проверяем, что пришел новый тик нового бара
                                    }
                                    //} Вызываем on_tick
                                }

                                //{ Вызываем on_test
                                if (m_config.on_test &&
                                    date_ms >= start_date_ms &&
                                    !m_internal_config.period_id[i].empty()) {
                                    if (!m_config.use_new_tick_mode) {
                                        m_config.on_test(s, t_ms, m_internal_config.period_id[i]);
                                    } else {
                                        if (is_new_tick) {
                                            m_config.on_test(s, t_ms, m_internal_config.period_id[i]);
                                            is_new_tick = false;
                                        }
                                    }
                                }
                                //} Вызываем on_test
                            } // for i
                        } // for date_ms
                        if (m_config.on_end_test_symbol) m_config.on_end_test_symbol(s);
                    }; // for s
                    if (m_config.on_end_test_thread) m_config.on_end_test_thread(n, number_threads);
                    //} Проходимся по всем символам
                });
            }; // for n
            m_async_tasks.wait();
            if (m_config.on_end_test) m_config.on_end_test();
        }

        // ВНИМАНИЕ!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        // Методы, представленные дальше, нужно испольтзовать только внутри callback-функций:
        // on_candle
        // on_tick
        // on_test
        // Это связано с тем, что для каждого рабочего потока есть своя копия базы данных, с которой нужно работать

        inline bool get_candle(
                Candle &candle,
                const size_t s_index,
                const uint64_t t,
                const QDB_TIMEFRAMES p = QDB_TIMEFRAMES::PERIOD_M1,
                const QDB_CANDLE_MODE m = QDB_CANDLE_MODE::SRC_CANDLE) noexcept {
            size_t thread_index = 0;
            if (!m_internal_config.get_thread_index(thread_index)) return false;
            if (m_symbol_db.empty()) return false;
            if (!m_symbol_db[thread_index]) return false;
            return m_symbol_db[thread_index]->get_candle(candle, s_index, t, p, m);
        }

        inline bool get_candle(
                Candle &candle,
                const std::string &symbol,
                const uint64_t t,
                const QDB_TIMEFRAMES p = QDB_TIMEFRAMES::PERIOD_M1,
                const QDB_CANDLE_MODE m = QDB_CANDLE_MODE::SRC_CANDLE) noexcept {
            auto it = m_internal_config.symbol_to_index.find(symbol);
            if (it == m_internal_config.symbol_to_index.end()) return false;
            return get_candle(candle, it->second, t, p, m);
        }

        inline bool get_tick(Tick &tick, const size_t s_index, const uint64_t t) noexcept {
            size_t thread_index = 0;
            if (!m_internal_config.get_thread_index(thread_index)) return false;
            if (m_symbol_db.empty()) return false;
            if (!m_symbol_db[thread_index]) return false;
            return m_symbol_db[thread_index]->get_tick(tick, s_index, t);
        }

        inline bool get_tick(Tick &tick, const std::string &symbol, const uint64_t t) noexcept {
            auto it = m_internal_config.symbol_to_index.find(symbol);
            if (it == m_internal_config.symbol_to_index.end()) return false;
            return get_tick(tick, it->second, t);
        }

        inline bool get_tick_ms(Tick &tick, const size_t s_index, const uint64_t t_ms) noexcept {
            size_t thread_index = 0;
            if (!m_internal_config.get_thread_index(thread_index)) return false;
            if (m_symbol_db.empty()) return false;
            if (!m_symbol_db[thread_index]) return false;
            return m_symbol_db[thread_index]->get_tick_ms(tick, s_index, t_ms);
        }

        inline bool get_tick_ms(Tick &tick, const std::string &symbol, const uint64_t t_ms) noexcept {
            auto it = m_internal_config.symbol_to_index.find(symbol);
            if (it == m_internal_config.symbol_to_index.end()) return false;
            return get_tick_ms(tick, it->second, t_ms);
        }

        inline bool get_next_tick_ms(Tick &tick, const size_t s_index, const uint64_t t_ms, const uint64_t t_ms_max) noexcept {
            size_t thread_index = 0;
            if (!m_internal_config.get_thread_index(thread_index)) return false;
            if (m_symbol_db.empty()) return false;
            if (!m_symbol_db[thread_index]) return false;
            return m_symbol_db[thread_index]->get_next_tick_ms(tick, s_index, t_ms, t_ms_max);
        }

        inline bool get_next_tick_ms(Tick &tick, const std::string &symbol, const uint64_t t_ms, const uint64_t t_ms_max) noexcept {
            auto it = m_internal_config.symbol_to_index.find(symbol);
            if (it == m_internal_config.symbol_to_index.end()) return false;
            return get_next_tick_ms(tick, it->second, t_ms, t_ms_max);
        }

        inline bool get_min_max_date(const bool use_tick_data, uint64_t &t_min, uint64_t &t_max) {
            size_t thread_index = 0;
            if (!m_internal_config.get_thread_index(thread_index)) return false;
            if (m_symbol_db.empty()) return false;
            if (!m_symbol_db[thread_index]) return false;
            return m_symbol_db[thread_index]->get_min_max_date(use_tick_data, t_min, t_max);
        }
    };
};

#endif // TRADING_DB_QDB_FX_HISTORY_HPP_INCLUDED
