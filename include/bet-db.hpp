#pragma once
#ifndef BET_DB_HPP_INCLUDED
#define BET_DB_HPP_INCLUDED

#include <sqlite_orm/sqlite_orm.h>
#include <xtime.hpp>
#include <vector>
#include <string>
#include <algorithm>

namespace trading_db {

    /** \brief Класс базы даных ставок БО
     */
    class BetDB {
    public:

        /** \brief Состояния ставки БО
         */
        enum class BetStatus {
            UNKNOWN_STATE = 0,                      /// Неопределенное состояние
            OPENING_ERROR = -3,                     /// Ошибка открытия
            CHECK_ERROR = -4,                       /// Ошибка проверки результата сделки
            WAITING_COMPLETION = -5,                /// Ожидание завершения сделки
            WIN = 1,                               	/// Удачная сделка
            LOSS = -1,                              /// Неудачная сделка
            STANDOFF = -2,							/// Ничья
        };

        /** \brief Типы контрактов ставок
         */
        enum class BetContractType {
            UNKNOWN_STATE = 0,
            BUY = 1,
            SELL = -1
        };

        enum class BetType {
            SPRINT = 0,
            CLASSIC = 1,
        };

        class BetConfig;

    private:

        /** \brief Структура заметок
         */
        struct Note {
            std::string key;        /// Ключ
            std::string value;      /// Значение
        };

        /** \brief Класс для хранения данных ставок в БД
         */
        class BetRawConfig {
        private:

            const size_t max_data_size = 6 * sizeof(double) + sizeof(int64_t) +
                    sizeof(uint32_t) + sizeof(float) + 3 * sizeof(int8_t);

            template<class T>
            inline void set_value(const T value, std::vector<char> &data, const size_t pos) {
                if(data.size() != max_data_size) data.resize(max_data_size);
                std::memcpy(&data[pos], &value, std::min(data.size(), sizeof(T)));
            }

            template<class T>
            inline T get_value(const std::vector<char> &data, const size_t pos) {
                T value = 0;
                std::memcpy(&value, &data[pos], std::min(data.size(), sizeof(T)));
                return value;
            }

        public:
            int	id = -1;            /// ключ - уникальный ID сделки в БД
            double open_date; 		/// метка времени открытия сделки

            std::string symbol;	    /// имя символа(валютная пара, акции, индекс и пр., например EURUSD)
            std::string broker;		/// имя брокера
            std::string currency;	/// валюта ставки
            std::string signal;		/// имя сигнала, стратегии или индикатора, короче имя источника сигнала
            std::string type;		/// тип бинарного опциона(SPRINT, CLASSIC и т.д., у разных брокеров разное, нужно чтобы это поле просто было)
            std::string comment;	/// комментарий

            std::vector<char> data; /// Данные сделки

            bool demo;              /// флаг демо аккаунта

            inline double get_close_date() { return get_value<double>(data, 0 * sizeof(double));};
            inline double get_open_price() { return get_value<double>(data, 1 * sizeof(double));};
            inline double get_close_price() { return get_value<double>(data, 2 * sizeof(double));};
            inline double get_amount() { return get_value<double>(data, 3 * sizeof(double));};
            inline double get_profit() { return get_value<double>(data, 4 * sizeof(double));};
            inline double get_payout() { return get_value<double>(data, 5 * sizeof(double));};
            inline int64_t get_broker_id() { return get_value<int64_t>(data, 6 * sizeof(double));};
            inline uint32_t get_duration() { return get_value<uint32_t>(data, 6 * sizeof(double) + sizeof(int64_t));};
            inline float get_delay() { return get_value<float>(data, 6 * sizeof(double) + sizeof(int64_t) + sizeof(uint32_t));};

            inline BetContractType get_contract_type() {
                const size_t offset = 6 * sizeof(double) + sizeof(int64_t) +
                    sizeof(uint32_t) + sizeof(float) + 0 * sizeof(int8_t);
                const int8_t temp = get_value<int8_t>(data, offset);
                return static_cast<BetContractType>(temp);
            };

            inline BetStatus get_status() {
                const size_t offset = 6 * sizeof(double) + sizeof(int64_t) +
                    sizeof(uint32_t) + sizeof(float) + 1 * sizeof(int8_t);
                const int8_t temp = get_value<int8_t>(data, offset);
                return static_cast<BetStatus>(temp);
            };

            inline BetType get_type() {
                const size_t offset = 6 * sizeof(double) + sizeof(int64_t) +
                    sizeof(uint32_t) + sizeof(float) + 2 * sizeof(int8_t);
                const int8_t temp = get_value<int8_t>(data, offset);
                return static_cast<BetType>(temp);
            };

            inline void set_close_date(const double value) { set_value(value, data, 0 * sizeof(double)); };
            inline void set_open_price(const double value) { set_value(value, data, 1 * sizeof(double)); };
            inline void set_close_price(const double value) { set_value(value, data, 2 * sizeof(double)); };
            inline void set_amount(const double value) { set_value(value, data, 3 * sizeof(double)); };
            inline void set_profit(const double value) { set_value(value, data, 4 * sizeof(double)); };
            inline void set_payout(const double value) { set_value(value, data, 5 * sizeof(double)); };
            inline void set_broker_id(const int64_t value) { set_value(value, data, 6 * sizeof(double)); };
            inline void set_duration(const uint32_t value) { set_value(value, data, 6 * sizeof(double) + sizeof(int64_t)); };
            inline void set_delay(const float value) { set_value(value, data, 6 * sizeof(double) + sizeof(int64_t) + sizeof(uint32_t)); };

            inline void set_contract_type(const BetContractType value) {
                const size_t offset = 6 * sizeof(double) + sizeof(int64_t) +
                    sizeof(uint32_t) + sizeof(float) + 0 * sizeof(int8_t);
                set_value<int8_t>(static_cast<int8_t>(value), data, offset);
            }

            inline void set_status(const BetStatus value) {
                const size_t offset = 6 * sizeof(double) + sizeof(int64_t) +
                    sizeof(uint32_t) + sizeof(float) + 1 * sizeof(int8_t);
                set_value<int8_t>(static_cast<int8_t>(value), data, offset);
            }

            inline void set_type(const BetType value) {
                const size_t offset = 6 * sizeof(double) + sizeof(int64_t) +
                    sizeof(uint32_t) + sizeof(float) + 2 * sizeof(int8_t);
                set_value<int8_t>(static_cast<int8_t>(value), data, offset);
            }

            BetRawConfig() {};

            BetRawConfig(const BetConfig &config) {
                id = config.id;
                open_date = config.open_date;
                const double close_date =
                    config.close_date == 0 ?
                    (config.open_date + config.duration) : config.close_date;
                set_close_date(close_date);
                set_open_price(config.open_price);
                set_close_price(config.close_price);

                set_amount(config.amount);
                set_profit(config.profit);
                set_payout(config.payout);

                set_broker_id(config.broker_id);

                const uint32_t duration =
                    config.duration == 0 ?
                    (close_date - config.open_date) : config.duration;
                set_duration(duration);

                set_delay(config.delay);
                demo = config.demo;

                set_contract_type(config.contract_type);
                set_status(config.status);
                set_type(config.type);

                symbol = config.symbol;
                broker = config.broker;
                currency = config.currency;
                signal = config.signal;
                comment = config.comment;
            }

            BetRawConfig &operator=(const BetRawConfig &config) {
                if(this != &config) {
                    id = config.id;
                    open_date = config.open_date;
                    symbol = config.symbol;
                    broker = config.broker;
                    currency = config.currency;
                    signal = config.signal;
                    comment = config.comment;
                    data = config.data;
                    demo = config.demo;
                }
                return *this;
            }
        };

    public:

        /** \brief Конфигурация ставки БО
         */
        class BetConfig {
        public:
            int	id = -1;            /// ключ - уникальный ID сделки в БД
            double open_date; 		/// метка времени открытия сделки
            double close_date; 		/// метка времени закрытия сделки
            double open_price;		/// цена входа в сделку
            double close_price;		/// цена выхода из сделки

            double amount; 			/// размер ставки
            double profit;			/// размер выплаты
            double payout;			/// процент выплат

            int64_t broker_id;		/// уникальный номер сделки, который присваивает брокер
            uint32_t duration; 		/// экспирация(длительность) бинарного опциона в секундах
            float delay; 			/// задержка на открытие ставки в секундах
            bool demo;				/// флаг демо аккаунта

            BetContractType contract_type;  /// тип контракта, см.BetContractType
            BetStatus status;       /// состояние сделки, см.BetStatus
            BetType type;           /// тип бинарного опциона(SPRINT, CLASSIC и т.д.), см.BetType

            std::string symbol;	    /// имя символа(валютная пара, акции, индекс и пр., например EURUSD)
            std::string broker;		/// имя брокера
            std::string currency;	/// валюта ставки
            std::string signal;		/// имя сигнала, стратегии или индикатора, короче имя источника сигнала
            std::string comment;	/// комментарий

            BetConfig() {};

            BetConfig(BetRawConfig &config) {
                id = config.id;
                open_date = config.open_date;
                close_date = config.get_close_date();
                open_price = config.get_open_price();
                close_price = config.get_close_price();

                amount = config.get_amount();
                profit = config.get_profit();
                payout = config.get_payout();

                broker_id = config.get_broker_id();
                duration = config.get_duration();
                delay = config.get_delay();
                demo = config.demo;

                contract_type = config.get_contract_type();
                status = config.get_status();
                type = config.get_type();

                symbol = config.symbol;
                broker = config.broker;
                currency = config.currency;
                signal = config.signal;
                comment = config.comment;
            }
        };

    private:

        /* столбец таблицы */
        template<class O, class T, class ...Op>
        using Column = sqlite_orm::internal::column_t<O, T, const T& (O::*)() const, void (O::*)(T), Op...>;

        /* тип хранилища
         *
         * Внимание! Внимание! Внимание!
         *
         * Компилятор mingw вылетает, когда столбцов в таблице становится от 15 шт.
         * Поэтому было принято решение упаковывать данные сделки в data
         */
        using Storage = sqlite_orm::internal::storage_t<
            sqlite_orm::internal::table_t<BetRawConfig,
                Column<BetRawConfig, decltype(BetRawConfig::id),
                sqlite_orm::constraints::unique_t<>,
                sqlite_orm::constraints::primary_key_t<>>,
                Column<BetRawConfig, decltype(BetRawConfig::open_date)>,
                Column<BetRawConfig, decltype(BetRawConfig::symbol)>,
                Column<BetRawConfig, decltype(BetRawConfig::broker)>,
                Column<BetRawConfig, decltype(BetRawConfig::currency)>,
                Column<BetRawConfig, decltype(BetRawConfig::signal)>,
                Column<BetRawConfig, decltype(BetRawConfig::comment)>,
                Column<BetRawConfig, decltype(BetRawConfig::data)>,
                Column<BetRawConfig, decltype(BetRawConfig::demo)>
            >,
            sqlite_orm::internal::table_t<Note,
                Column<Note, decltype(Note::key), sqlite_orm::constraints::primary_key_t<>>,
                Column<Note, decltype(Note::value)>
            >
        >;

        Storage storage;                        /**< БД sqlite */

        /** \brief Инициализировать хранилище
         * \bug Компилятор mingw вылетает, когда столбцов в таблице становится от 15 шт.
         * Поэтому было принято решение упаковывать данные сделки в data
         * \param path Путь к БД
         * \return Возвращает базу данных sqlite orm
         */
        inline auto init_storage(const std::string &path) {
            return sqlite_orm::make_storage(path,
                sqlite_orm::make_table("Bets",
                    sqlite_orm::make_column("id", &BetRawConfig::id, sqlite_orm::unique(), sqlite_orm::primary_key()),
                    sqlite_orm::make_column("open_date", &BetRawConfig::open_date),
                    sqlite_orm::make_column("symbol", &BetRawConfig::symbol),
                    sqlite_orm::make_column("broker", &BetRawConfig::broker),
                    sqlite_orm::make_column("currency", &BetRawConfig::currency),
                    sqlite_orm::make_column("signal", &BetRawConfig::signal),
                    sqlite_orm::make_column("comment", &BetRawConfig::comment),
                    sqlite_orm::make_column("data", &BetRawConfig::data),
                    sqlite_orm::make_column("demo", &BetRawConfig::demo)),
                sqlite_orm::make_table("Notes",
                    sqlite_orm::make_column("key", &Note::key, sqlite_orm::primary_key()),
                    sqlite_orm::make_column("value", &Note::value)));
		}

        /** \brief Инициализация объектов класса
         */
		inline void init_other() {
            storage.sync_schema();
		}

        size_t write_buffer_size = 100;         /**< Размер буфера для записи */
        std::vector<BetRawConfig> write_buffer; /**< Буфер для накопления данных для записи в БД */

        /** \brief Записать буфер для записи в БД
         */
		inline void write_buffer_to_db() {
            if (!write_buffer.empty()) {
                /* если буфер для записи не пустой, сначала запишем его */
                storage.transaction([&] {
                    for (BetRawConfig& b : write_buffer) {
                        storage.insert(b);
                    }
                    return true;
                });
                write_buffer.clear();
            }
		}

		inline void erase_array(
                const uint32_t start_time,
                const uint32_t stop_time,
                std::vector<BetRawConfig> &raw_deals) {
            if(start_time != 0 && stop_time != 0) {
                size_t pos = 0;
                while(pos < raw_deals.size()) {
                    const uint32_t second_day = xtime::get_second_day(raw_deals[pos].open_date);
                    if (second_day < start_time || second_day > stop_time) {
                        raw_deals.erase(raw_deals.begin() + pos);
                    } else {
                        ++pos;
                    }
                }
            }
		}

    public:

        /** \brief Конструктор базы данных ставок БО
         * \param path Путь к файлу БД
         */
        BetDB(const std::string& path) : storage(init_storage(path)) {
            init_other();
        };

        ~BetDB() {
            /* записываем в БД из буфера, если есть не записанные данные */
            write_buffer_to_db();
        };

        /** \brief Установить размер буфера для записи
         * \param value Значение размера буфера
         */
        inline void set_write_buffer_size(const size_t value) {
            write_buffer_size = value;
        };

        /** \brief Установить значение заметки по ключу в БД
         * \param key Ключ
         * \param value Значение
         */
		void set_note(const std::string& key, const std::string& value) {
		    Note kv{key, value};
            storage.replace(kv);
		}

        /** \brief Получить значение заметки по ключу из БД
         * \param key Ключ
         * \return Значение
         */
        std::string get_note(const std::string& key) {
            if (auto kv = storage.get_pointer<Note>(key)) {
                return kv->value;
            }
            return std::string();
        };

        /** \brief Добавить ставку БО в БД
         * \warning Данная функция сама запишет данные по достижению нужного размера буфера для записи
         * \param bet Структура БО
         */
        void add(const BetConfig &bet) {
            write_buffer.push_back(BetRawConfig(bet));
            if (write_buffer.size() >= write_buffer_size) {
                write_buffer_to_db();
            }
        };

        /** \brief Записать буфер в БД и очистить его
         */
        void flush() {
            write_buffer_to_db();
        }

        /** \brief Очистить БД от данных ставок
         */
        void clear() {
            storage.remove_all<BetRawConfig>();
        };

        /** \brief Очистить заметки БД
         */
        void clear_notes() {
            storage.remove_all<Note>();
        };

        /** \brief Очистить БД от данных ставок и заметок
         */
        void clear_all() {
            storage.remove_all<BetRawConfig>();
            storage.remove_all<Note>();
        };

        /** \brief Очистить данные сделок БО по указаным критериям
         * \param start_date Начальная дата. Если 0, то не используется
         * \param stop_date Конечная дата. Если 0, то не используется
         * \param symbol_name Имя символа. Если пустая строка - не используется
         * \param signal_name Имя сигнала. Если пустая строка - не используется
         * \param broker_name Имя брокера. Если пустая строка - не используется
         * \param use_demo Флаг, включает удаление сделок на демо счете
         * \param use_real Флаг, включает удаление сделок на реальном счете
         */
        void clear(
                const double start_date,
                const double stop_date,
                const std::string& symbol_name = std::string(),
                const std::string& signal_name = std::string(),
                const std::string& broker_name = std::string(),
                const bool use_demo = true,
                const bool use_real = true) {
            using namespace sqlite_orm;
            write_buffer_to_db();
            storage.remove_all<BetRawConfig>(where(
                (start_date == 0 || c(&BetRawConfig::open_date) >= start_date) &&
                (stop_date == 0 || c(&BetRawConfig::open_date) <= stop_date) &&
                (signal_name.empty() || c(&BetRawConfig::signal) == signal_name) &&
                (broker_name.empty() || c(&BetRawConfig::broker) == broker_name) &&
                (symbol_name.empty() || c(&BetRawConfig::symbol) == symbol_name) &&
                (c(&BetRawConfig::demo) == use_demo ||
                 c(&BetRawConfig::demo) == !use_real)));
        };

        /** \brief Получить массив сделок БО по указанным критериям
         * \param start_date Начальная дата. Если 0, то не используется
         * \param stop_date Конечная дата. Если 0, то не используется
         * \param symbol_name Имя символа. Если пустая строка - не используется
         * \param signal_name Имя сигнала. Если пустая строка - не используется
         * \param broker_name Имя брокера. Если пустая строка - не используется
         * \param use_demo Флаг, включает поиск сделок на демо счете
         * \param use_real Флаг, включает поиск сделок на реальном счете
         * \param start_time Начальное время дня для первого периода фильтра, в секундах. Если 0, то не используется
         * \param stop_time Конечное время дня для первого периода фильтра, в секундах. Если 0, то не используется
         * \param start_time2 Начальное время дня для первого периода фильтра, в секундах. Если 0, то не используется
         * \param stop_time2 Конечное время дня для первого периода фильтра, в секундах. Если 0, то не используется
         */
        std::vector<BetConfig> get_array(
                const double start_date,
                const double stop_date,
                const std::string& symbol_name = std::string(),
                const std::string& signal_name = std::string(),
                const std::string& broker_name = std::string(),
                const bool use_demo = true,
                const bool use_real = true,
                const uint32_t start_time = 0,
                const uint32_t stop_time = 0,
                const uint32_t start_time2 = 0,
                const uint32_t stop_time2 = 0) {
            using namespace sqlite_orm;
            if (start_date == 0 || stop_date == 0) return std::vector<BetConfig>();

            /* сначала запишем данные, если они есть */
            write_buffer_to_db();

            std::vector<BetRawConfig> raw_deals = storage.get_all<BetRawConfig>(where(
                    (start_date == 0 || c(&BetRawConfig::open_date) >= start_date) &&
                    (stop_date == 0 || c(&BetRawConfig::open_date) <= stop_date) &&
                    (signal_name.empty() || c(&BetRawConfig::signal) == signal_name) &&
                    (broker_name.empty() || c(&BetRawConfig::broker) == broker_name) &&
                    (symbol_name.empty() || c(&BetRawConfig::symbol) == symbol_name) &&
                    (c(&BetRawConfig::demo) == use_demo ||
                     c(&BetRawConfig::demo) == !use_real)));

            erase_array(start_time, stop_time, raw_deals);
            erase_array(start_time2, stop_time2, raw_deals);

            if(raw_deals.empty()) return std::vector<BetConfig>();

            /* преобразуем массив сырых данных в массив сделок */
            std::vector<BetConfig> deals;
            deals.reserve(raw_deals.size());
            std::transform(raw_deals.begin(), raw_deals.end(),
                    std::back_inserter(deals),
                    [](BetRawConfig &config) -> BetConfig {
                return BetConfig(config);
            });
            return deals;
        };

        /** \brief Получить винрейт сделок
         * \param deals Массив сделок
         * \return Винрейт сделок
         */
        static double get_winrate(const std::vector<BetConfig> &deals) {
            const uint32_t wins = std::count_if(deals.begin(), deals.end(),
                    [&](const BetConfig &config){
                return config.status == BetStatus::WIN;
            });
            const uint32_t losses = std::count_if(deals.begin(), deals.end(),
                    [&](const BetConfig &config){
                return config.status == BetStatus::LOSS;
            });
            const uint32_t all_deals = wins + losses;
            return all_deals == 0 ? 0 : (double)wins / (double)all_deals;
        }

        /** \brief Получить совокупный профит от сделок
         * \param deals Массив сделок
         * \return Профит от сделок
         */
        static double get_total_profit(const std::vector<BetConfig> &deals) {
            double total_profit = 0;
            std::for_each(deals.begin(), deals.end(),
                    [&](const BetConfig &config){
                switch(config.status) {
                case BetStatus::WIN:
                    total_profit += config.profit;
                    break;
                case BetStatus::LOSS:
                    total_profit -= config.amount;
                    break;
                case BetStatus::STANDOFF:
                    break;
                default:
                    break;
                };
            });
            return total_profit;
        }
    };
}

#endif // BET_DB_HPP_INCLUDED
