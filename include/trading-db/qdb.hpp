#pragma once
#ifndef TRADING_DB_QDB_HPP_INCLUDED
#define TRADING_DB_QDB_HPP_INCLUDED

#if SQLITE_THREADSAFE != 1
#error "The project must be built for sqlite multithreading! Set the SQLITE_THREADSAFE=1"
#endif

#include "config.hpp"

#include "parts/qdb/enums.hpp"
#include "parts/qdb/data-classes.hpp"
#include "parts/qdb/data-preparation.hpp"
#include "parts/qdb/price-buffer.hpp"
#include "parts/qdb/writer-price-buffer.hpp"
#include "parts/qdb/storage.hpp"
#include "tools/qdb/csv.hpp"

#include "utils/sqlite-func.hpp"
#include "utils/async-tasks.hpp"
#include "utils/safe-queue.hpp"
#include "utils/print.hpp"
#include "utils/files.hpp"

#include <ztime.hpp>
#include <mutex>
#include <atomic>
#include <future>
#include <vector>
#include <map>
#include <set>

namespace trading_db {

    /** \brief Class for storing quotes
     */
    class QDB {
    public:

        /** \brief Class configuration for storing quotes
         */
        class Config {
        public:
            std::string symbol;         /**< Symbol name (optional) */
            std::string source;         /**< Quote data source (optional) */
            int         digits  = 0;    /**< The number of decimals */
            bool        use_data_merge = false; /**< Use data merge mode */

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

		bool read_ticks(const uint64_t t, std::map<uint64_t, ShortTick> &ticks) {
            std::vector<uint8_t> data;
            if (!storage.read_ticks(data, t)) {
                print_error("error read ticks", __LINE__);
                return false;
            }
            data_preparation.config.price_scale = config.digits;
            if (!data_preparation.decompress_ticks(t, data, ticks)) {
                print_error("error decompress ticks", __LINE__);
                return false;
            }
            return true;
		}

		bool read_candles(const uint64_t t, std::array<trading_db::Candle, ztime::MIN_PER_DAY> &candles) {
            std::vector<uint8_t> data;
            if (!storage.read_candles(data, t)) {
                print_error("error read candles", __LINE__);
                return false;
            }
            data_preparation.config.price_scale = config.digits;
            if (!data_preparation.decompress_candles(t, data, candles)) {
                print_error("error decompress candles", __LINE__);
                return false;
            }
            return true;
		}

		bool compress_ticks(
                const uint64_t t,
                const std::map<uint64_t, ShortTick> &ticks,
                std::vector<uint8_t> &data) {
            data_preparation.config.price_scale = config.digits;
            if (!data_preparation.compress_ticks(t, ticks, data)) {
                print_error("error compress ticks", __LINE__);
                return false;
            }
            return true;
		}

		bool compress_candles(
                const std::array<trading_db::Candle, ztime::MIN_PER_DAY> &candles,
                std::vector<uint8_t> &data) {
            data_preparation.config.price_scale = config.digits;
            if (!data_preparation.compress_candles(candles, data)) {
                print_error("error compress candles", __LINE__);
                return false;
            }
            return true;
		}

        void init() {

            //{ initialize data record
            writer_buffer.on_ticks = [&](
                    const std::map<uint64_t, trading_db::ShortTick> &ticks,
                    const uint64_t t) {
                const uint64_t start_time = ztime::start_of_hour(t);
                std::map<uint64_t, ShortTick> new_ticks(ticks);
                if (config.use_data_merge) {
                    std::map<uint64_t, ShortTick> prev_ticks;
                    if (read_ticks(start_time, prev_ticks)) {
                        new_ticks.insert(prev_ticks.begin(), prev_ticks.end());
                    }
                }
                std::vector<uint8_t> data;
                if (compress_ticks(start_time, new_ticks, data)) {
                    if (!data.empty()) write_ticks_buffer[start_time] = data;
                }
            };

            writer_buffer.on_candles = [&](
                    const std::array<trading_db::Candle, ztime::MIN_PER_DAY> &candles,
                    const uint64_t t) {
                const uint64_t start_time = ztime::start_of_day(t);
                std::array<trading_db::Candle, ztime::MIN_PER_DAY> new_candles(candles);
                if (config.use_data_merge) {
                    std::array<trading_db::Candle, ztime::MIN_PER_DAY> prev_candles;
                    if (read_candles(start_time, prev_candles)) {
                        for (size_t i = 0; i < ztime::MIN_PER_DAY; ++i) {
                            if (!new_candles[i].empty()) prev_candles[i] = new_candles[i];
                        }
                        new_candles = prev_candles;
                    }
                }
                std::vector<uint8_t> data;
                if (compress_candles(new_candles, data)) {
                    if (!data.empty()) write_candles_buffer[start_time] = data;
                }
            };
            //}

            //{ initialize reading
            price_buffer.on_read_ticks = [&](const uint64_t t) -> std::map<uint64_t, trading_db::ShortTick> {
                std::map<uint64_t, ShortTick> temp;
                if (!read_ticks(t, temp)) {
                    print_error("error read ticks [price_buffer]", __LINE__);
                }
                return temp;
            };

            price_buffer.on_read_candles = [&](const uint64_t t) -> std::array<trading_db::Candle, ztime::MIN_PER_DAY> {
                std::array<trading_db::Candle, ztime::MIN_PER_DAY> temp;
                if (!read_candles(t, temp)) {
                    print_error("error read candles [price_buffer]", __LINE__);
                }
                return temp;
            };
            //}

        }

    public:

        using METADATA_TYPE = QdbStorage::METADATA_TYPE;

        QDB() {init();}
        ~QDB() {}

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
            return storage.remove_candles(ztime::start_of_day(t));
        }

        inline bool remove_ticks(const uint64_t t) noexcept {
            return storage.remove_ticks(ztime::start_of_hour(t));
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

		inline bool set_info_str(const QdbStorage::METADATA_TYPE type, const std::string &value) noexcept {
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

		inline bool set_info_int(const QdbStorage::METADATA_TYPE type, const int value) noexcept {
            switch (type) {
            case QdbStorage::METADATA_TYPE::SYMBOL_DIGITS:
                config.digits = value;
                break;
            default:
                return false;
            };
            return storage.set_info_int(type, value);
		}

		inline bool get_min_max_date(const bool use_tick_data, uint64_t &t_min, uint64_t &t_max) {
            return storage.get_min_max_date(use_tick_data, t_min, t_max);
		}

        //----------------------------------------------------------------------
        // методы для чтения данных

        inline bool get_candle(Candle &candle,
                const uint64_t t,
                const QDB_TIMEFRAMES p = QDB_TIMEFRAMES::PERIOD_M1,
                const QDB_CANDLE_MODE m = QDB_CANDLE_MODE::SRC_CANDLE) noexcept {
            return price_buffer.get_candle(candle, t, p, m);
        }

        inline bool get_tick(Tick &tick, const uint64_t t) noexcept {
            return price_buffer.get_tick(tick, t);
        }

        inline bool get_tick_ms(Tick &tick, const uint64_t t_ms) noexcept {
            return price_buffer.get_tick_ms(tick, t_ms);
        }

        inline bool get_next_tick_ms(Tick &tick, const uint64_t t_ms, const uint64_t t_ms_max) noexcept {
            return price_buffer.get_next_tick_ms(tick, t_ms, t_ms_max);
        }
    };

};

#endif // TRADING_DB_QDB_HPP_INCLUDED
