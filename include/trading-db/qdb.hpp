#pragma once
#ifndef TRADING_DB_QDB_HPP_INCLUDED
#define TRADING_DB_QDB_HPP_INCLUDED

#if SQLITE_THREADSAFE != 1
#error "The project must be built for sqlite multithreading! Set the SQLITE_THREADSAFE=1"
#endif

#include "config.hpp"

#include "parts/qdb-common.hpp"
#include "parts/qdb-data-preparation.hpp"
#include "parts/qdb-price-buffer.hpp"
#include "parts/qdb-writer-price-buffer.hpp"
#include "parts/qdb-storage.hpp"
#include "parts/qdb-csv.hpp"
//#include "parts/qdb-history.hpp"

#include "utility/sqlite-func.hpp"
#include "utility/async-tasks.hpp"
#include "utility/safe-queue.hpp"
#include "utility/print.hpp"
#include "utility/files.hpp"

#include <ztime.hpp>
#include <mutex>
#include <atomic>
#include <future>
#include <vector>
#include <map>
#include <set>

namespace trading_db {

    /** \brief Класс для хранения котировок
     */
    class QDB {
    public:

        /** \brief Конфигурация класса для хранения котировок
         */
        class Config {
        public:
            std::string symbol;         /**< Имя символа */
            std::string source;         /**< Источник данных котировок */
            int         digits  = 0;    /**< Количество знаков после запятой */

            std::string title = "qdb: ";
            bool        use_log = false;
        } config;

    private:
        QdbPriceBuffer          price_buffer;
        QdbStorage              storage;
        QdbDataPreparation      data_preparation;
        QdbWriterPriceBuffer    writer_buffer;

        std::map<uint64_t, std::vector<uint8_t>> write_ticks_buffer;
        std::map<uint64_t, std::vector<uint8_t>> write_candles_buffer;

        inline void print_error(
				const std::string message,
				const int line) noexcept {
			if (config.use_log) {
				TRADING_DB_PRINT
					<< config.title << "error in [file " << __FILE__
					<< ", line " << line
					<< "], message: " << message << std::endl;
			}
		}

        void init() {

            // инициализируем запись данных
            writer_buffer.on_ticks = [&](
                    const std::map<uint64_t, trading_db::ShortTick> &ticks,
                    const uint64_t t) {
                const uint64_t start_time = ztime::get_first_timestamp_hour(t);

                std::vector<uint8_t> data;
                data_preparation.config.price_scale = config.digits;
                if (!data_preparation.compress_ticks(start_time, ticks, data)) {
                    print_error("error compress ticks", __LINE__);
                    return;
                }
                if (!data.empty()) write_ticks_buffer[start_time] = data;
            };

            writer_buffer.on_candles = [&](
                    const std::array<trading_db::Candle, ztime::MINUTES_IN_DAY> &candles,
                    const uint64_t t) {
                const uint64_t start_time = ztime::get_first_timestamp_day(t);

                // сжимаем данные
                std::vector<uint8_t> data;
                data_preparation.config.price_scale = config.digits;
                if (!data_preparation.compress_candles(candles, data)) {
                    print_error("error compress candles", __LINE__);
                    return;
                }
                if (!data.empty()) write_candles_buffer[start_time] = data;
            };

            // инициализируем чтение
            price_buffer.on_read_ticks = [&](const uint64_t t) -> std::map<uint64_t, trading_db::ShortTick> {
                std::map<uint64_t, ShortTick> temp;

                std::vector<uint8_t> data;
                if (!storage.read_ticks(data, t)) {
                    print_error("error read ticks", __LINE__);
                    return temp;
                }

                data_preparation.config.price_scale = config.digits;
                if (!data_preparation.decompress_ticks(t, data, temp)) {
                    print_error("error decompress ticks", __LINE__);
                    return temp;
                }
                return temp;
            };

            price_buffer.on_read_candles = [&](const uint64_t t) -> std::array<trading_db::Candle, ztime::MINUTES_IN_DAY> {
                std::array<trading_db::Candle, ztime::MINUTES_IN_DAY> temp;

                std::vector<uint8_t> data;
                if (!storage.read_candles(data, t)) {
                    print_error("error read ticks", __LINE__);
                    return temp;
                }

                data_preparation.config.price_scale = config.digits;
                if (!data_preparation.decompress_candles(t, data, temp)) {
                    print_error("error decompress candles", __LINE__);
                    return temp;
                }
                return temp;
            };

        }

    public:

        using METADATA_TYPE = QdbStorage::METADATA_TYPE;

        QDB() {
            init();
        }

        ~QDB() {

        }

        //----------------------------------------------------------------------

        /** \brief Открыть БД
		 * \param path		Путь к файлу БД
		 * \param readonly	Флаг 'только чтение'
		 * \return Вернет true в случае успешной инициализации
		 */
		inline bool open(const std::string &path, const bool readonly = false) noexcept {
			if (!storage.open(path, readonly)) return false;
			config.digits = storage.get_info_int(QdbStorage::METADATA_TYPE::SYMBOL_DIGITS);
			config.symbol = storage.get_info_str(QdbStorage::METADATA_TYPE::SYMBOL_NAME);
			config.source = storage.get_info_str(QdbStorage::METADATA_TYPE::SYMBOL_DATA_FEED_SOURCE);
			return true;
		}


        //----------------------------------------------------------------------
        // методы для записи данных

        inline void start_write() noexcept {
            write_ticks_buffer.clear();
            write_candles_buffer.clear();
            writer_buffer.start();
        }

        inline void write_candle(const Candle &candle) noexcept {
            writer_buffer.write(candle);
        };

        inline void write_tick(const Tick &tick) noexcept {
            writer_buffer.write(tick);
        };

        inline bool stop_write() noexcept {
            writer_buffer.stop();
            if (!write_candles_buffer.empty()) {
                if (!storage.write_candles(write_candles_buffer)) return false;
            }
            if (!write_ticks_buffer.empty()) {
                if (!storage.write_ticks(write_ticks_buffer)) return false;
            }
            return true;
        }

        inline bool remove_candles(const uint64_t t) noexcept {
            return storage.remove_candles(ztime::get_first_timestamp_day(t));
        }

        inline bool remove_ticks(const uint64_t t) noexcept {
            return storage.remove_ticks(ztime::get_first_timestamp_hour(t));
        }

        inline bool remove_all() noexcept {
			return storage.remove_all();
		}

		//----------------------------------------------------------------------

		inline std::string get_info_str(const QdbStorage::METADATA_TYPE type) noexcept {
            return storage.get_info_str(type);
		}

		inline int get_info_int(const QdbStorage::METADATA_TYPE type) noexcept {
            return storage.get_info_int(type);
		}

		inline bool set_info_str(const QdbStorage::METADATA_TYPE type, const std::string &value) {
            switch (type) {
            case QdbStorage::METADATA_TYPE::SYMBOL_NAME:
                config.symbol = value;
                break;
            case QdbStorage::METADATA_TYPE::SYMBOL_DATA_FEED_SOURCE:
                config.source = value;
                break;
            default:
                return false;
            };
            return storage.set_info_str(type, value);
		}

		inline bool set_info_int(const QdbStorage::METADATA_TYPE type, const int value) {
            switch (type) {
            case QdbStorage::METADATA_TYPE::SYMBOL_DIGITS:
                config.digits = value;
                break;
            default:
                return false;
            };
            return storage.set_info_int(type, value);
		}


        //----------------------------------------------------------------------
        // методы для чтения данных

        bool get_candle(Candle &candle,
                const uint64_t t,
                const QDB_TIMEFRAMES p = QDB_TIMEFRAMES::PERIOD_M1,
                const QDB_CANDLE_MODE m = QDB_CANDLE_MODE::SRC_CANDLE) {
            return price_buffer.get_candle(candle, t, p, m);
        }

        bool get_tick(Tick &tick, const uint64_t t) {
            return price_buffer.get_tick(tick, t);
        }

        bool get_tick_ms(Tick &tick, const uint64_t t_ms) {
            return price_buffer.get_tick_ms(tick, t_ms);
        }
    };

};

#endif // TRADING_DB_QDB_HPP_INCLUDED
