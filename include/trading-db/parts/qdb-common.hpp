#pragma once
#ifndef TRADING_DB_QDB_COMMON_HPP_INCLUDED
#define TRADING_DB_QDB_COMMON_HPP_INCLUDED

namespace trading_db {

    /// Режимы использования цены
    enum class QDB_PRICE_MODE {
        BID_PRICE,
        ASK_PRICE,
        AVG_PRICE
    };

    /// Режим получения данных свечи
    enum class QDB_CANDLE_MODE {
        SRC_TICK,
        SRC_CANDLE
    };

    /// Таймфреймы
    enum class QDB_TIMEFRAMES {
        PERIOD_M1   = 1,
        PERIOD_M5   = 5,
        PERIOD_M15  = 15,
        PERIOD_M30  = 30,
        PERIOD_H1   = 60,
        PERIOD_H4   = 240,
        PERIOD_D1   = 1440,
    };

    /** \brief Класс для хранения бара
     */
    class Candle {
    public:
        double      open;
        double      high;
        double      low;
        double      close;
        double      volume;
        uint64_t    timestamp;

        Candle() :
            open(0),
            high(0),
            low (0),
            close(0),
            volume(0),
            timestamp(0) {
        };

        Candle(
                const double &new_open,
                const double &new_high,
                const double &new_low,
                const double &new_close,
                const uint64_t &new_timestamp) :
            open(new_open),
            high(new_high),
            low (new_low),
            close(new_close),
            volume(0),
            timestamp(new_timestamp) {}

        Candle(
                const double &new_open,
                const double &new_high,
                const double &new_low,
                const double &new_close,
                const double &new_volume,
                const uint64_t &new_timestamp) :
            open(new_open),
            high(new_high),
            low (new_low),
            close(new_close),
            volume(new_volume),
            timestamp(new_timestamp) {}

        bool empty() const noexcept {
            return (timestamp == 0 || close == 0);
        }
    }; // Candle

    /** \brief Класс для хранения урезанных данных бара (без времени)
     */
    class ShortCandle {
    public:
        double      open;
        double      high;
        double      low;
        double      close;
        double      volume;

        ShortCandle() :
            open(0),
            high(0),
            low (0),
            close(0),
            volume(0) {
        };

        ShortCandle(
                const double &new_open,
                const double &new_high,
                const double &new_low,
                const double &new_close) :
            open(new_open),
            high(new_high),
            low (new_low),
            close(new_close),
            volume(0) {}

        ShortCandle(
                const double &new_open,
                const double &new_high,
                const double &new_low,
                const double &new_close,
                const double &new_volume) :
            open(new_open),
            high(new_high),
            low (new_low),
            close(new_close),
            volume(new_volume) {}

        bool empty() const noexcept {
            return (close == 0);
        }
    }; // ShortCandle

    /** \brief Класс для хранения тика
     */
    class Tick {
    public:
        double      bid;
        double      ask;
        uint64_t    timestamp_ms;

        Tick() :
            bid(0),
            ask(0),
            timestamp_ms(0) {
        };

        Tick(	const double &new_bid,
                const double &new_ask,
                const uint64_t &new_timestamp_ms) :
            bid(new_bid),
            ask(new_ask),
            timestamp_ms(new_timestamp_ms) {}

        bool empty() const noexcept {
            return (timestamp_ms == 0);
        }
    }; // Tick

    /** \brief Класс для хранения урезанных данных тика (без времени)
     */
    class ShortTick {
    public:
        double      bid;
        double      ask;

        ShortTick() :
            bid(0),
            ask(0) {
        };

        ShortTick(
                const double &new_bid,
                const double &new_ask) :
            bid(new_bid),
            ask(new_ask) {
        }

        bool empty() const noexcept {
            return (bid == 0);
        }
    }; // ShortTick
};

#endif // TRADING_DB_QDB_COMMON_HPP_INCLUDED
