#include <iostream>
#include "trading-db\parts\qdb\price-buffer.hpp"

inline double get_price(const uint64_t t) noexcept {
    const uint64_t temp = t % 100 + 100;
    return (double)temp / 100.0;
}

int main() {
    std::cout << "Hello world!" << std::endl;

    trading_db::QdbPriceBuffer buffer;

    buffer.on_read_ticks = [&](const uint64_t t) -> std::map<uint64_t, trading_db::ShortTick> {
        //std::cout << "on_read_ticks " << ztime::get_str_date_time(t) << std::endl;
        std::map<uint64_t, trading_db::ShortTick> temp;
        //if (ztime::get_hour_day(t) % 5 == 0) return temp;
        for (uint64_t i = t; i < (t + ztime::SEC_PER_HOUR); ++i) {
            if (i % 10 != 0) continue;
            const double price = get_price(i);
            const uint64_t t_ms = i * ztime::MS_PER_SEC;
            //std::cout << "gp " << price << " t " << ztime::get_str_date_time(i) << std::endl;
            temp[t_ms] = trading_db::ShortTick(price, price + 1);
        }
        return temp;
    };

    buffer.on_read_candles = [&](const uint64_t t) -> std::array<trading_db::Candle, ztime::MIN_PER_DAY> {
        //std::cout << "on_read_candles " << ztime::get_str_date_time(t) << std::endl;
        std::array<trading_db::Candle, ztime::MIN_PER_DAY> temp;
        for (uint64_t i = t; i < (t + ztime::SEC_PER_DAY); ++i) {
            if (i % 10 != 0) continue;
            const uint32_t md = ztime::get_minute_day(i);
            const double price = get_price(i);
            temp[md].timestamp = ztime::get_first_timestamp_minute(i);
            if (temp[md].empty()) {
                temp[md].open = temp[md].close = temp[md].high = temp[md].low = price;
            } else {
                if (price > temp[md].high) temp[md].high = price;
                if (price < temp[md].low) temp[md].low = price;
                temp[md].close = price;
            }
        }
        return temp;
    };

    // тестируем получение цен через в виде свечей
    for (uint64_t i = ztime::get_timestamp(1,1,2020,23,55,0); i < ztime::get_timestamp(1,1,2022,0,0,0); ++i) {
        if (i % 10 != 0) continue;
        trading_db::Candle candle;
        if (buffer.get_candle(candle, i, trading_db::QDB_TIMEFRAMES::PERIOD_M1, trading_db::QDB_CANDLE_MODE::SRC_TICK)) {
            /*
            std::cout
                << "o " << candle.open
                << " h " << candle.high
                << " l " << candle.low
                << " c " << candle.close
                << " t " << ztime::get_str_date_time(candle.timestamp)
                << " s " << ztime::get_str_time(i)
                << std::endl;
                */
            if (candle.close != get_price(i)) {
                std::cout << "price error (1): c " << candle.close << " gp " << get_price(i) << " t " << ztime::get_str_date_time(i) << std::endl;
            }
        } else {
            std::cout << "get_candle error (1): " << ztime::get_str_date_time(i) << std::endl;
        }
        if (i % 60 != 0) continue;
        if (buffer.get_candle(candle, i, trading_db::QDB_TIMEFRAMES::PERIOD_M1, trading_db::QDB_CANDLE_MODE::SRC_CANDLE)) {
            if (candle.close != get_price(i + 50)) {
                std::cout << "price error (2): c " << candle.close << " gp " << get_price(i + 50) << " t " << ztime::get_str_date_time(i) << std::endl;
            }
        } else {
            std::cout << "get_candle error (2):" << ztime::get_str_date_time(i) << std::endl;
        }
    }

    return 0;
}
