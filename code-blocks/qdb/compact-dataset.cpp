#include <iostream>
#include "../../include/trading-db/parts/qdb/compact-candles-dataset.hpp"
#include "../../include/trading-db/parts/qdb/compact-ticks-dataset.hpp"
#include "../../include/trading-db/parts/qdb/compact-dataset.hpp"
#include <map>

inline double get_price(const uint64_t t) noexcept {
    const uint64_t temp = t % 100 + 100;
    return (double)temp / 100.0;
}

inline bool cmp_equal(const double x, const double y, const double eps) noexcept {
    return std::fabs(x - y) < eps;
}

int main() {
    std::cout << "Hello world!" << std::endl;
    // проверяем данные баров
    {
        std::array<trading_db::Candle, ztime::MINUTES_IN_DAY> candles;
        const size_t price_scale = 5;
        const size_t volume_scale = 5;
        trading_db::QdbCompactDataset dataset;

        const uint64_t timestamp_start = ztime::get_timestamp(1,1,2021,0,0,0);

        for(size_t i = 0; i < ztime::MINUTES_IN_DAY; ++i) {
            const uint64_t t = timestamp_start + i * 60;
            trading_db::Candle candle;
            candle.open = get_price(t);
            candle.high = candle.open + 0.2;
            candle.low = candle.open - 0.2;
            candle.close = candle.open + 0.1;
            candle.volume = 100 + 0.1 * i;
            candle.timestamp = t;
            candles[i] = candle;
        }

        dataset.write_candles(candles, price_scale, volume_scale);

        std::array<trading_db::Candle, ztime::MINUTES_IN_DAY> r_candles;
        size_t r_price_scale = 0;
        size_t r_volume_scale = 0;

        dataset.read_candles(r_candles, r_price_scale, r_volume_scale, timestamp_start);

        for(size_t i = 0; i < r_candles.size(); ++i) {
            const uint64_t t = timestamp_start + i * 60;
            trading_db::Candle candle;
            candle.open = get_price(t);
            candle.high = candle.open + 0.2;
            candle.low = candle.open - 0.2;
            candle.close = candle.open + 0.1;
            candle.volume = 100 + 0.1 * i;
            candle.timestamp = t;

            const double eps = 0.000001;
            if (r_candles[i].timestamp != candle.timestamp) std::cout << "candle t error" << std::endl;
            if (!cmp_equal(r_candles[i].open, candle.open, eps)) std::cout << "candle open error" << std::endl;
            if (!cmp_equal(r_candles[i].high, candle.high, eps)) std::cout << "candle high error" << std::endl;
            if (!cmp_equal(r_candles[i].low, candle.low, eps)) std::cout << "candle low error" << std::endl;
            if (!cmp_equal(r_candles[i].close, candle.close, eps)) std::cout << "candle close error" << std::endl;
            if (!cmp_equal(r_candles[i].volume, candle.volume, eps)) std::cout << "candle volume error" << std::endl;

            /*
            std::cout
                << "o " << r_candles[i].open
                << " h " << r_candles[i].high
                << " l " << r_candles[i].low
                << " c " << r_candles[i].close
                << " t " << ztime::get_str_date_time(r_candles[i].timestamp) << std::endl;
            */
        }

        auto &data = dataset.get_data();
        std::cout << "data size: " << data.size() << std::endl;
    }
    // проверяем данные тиков
    {
        std::map<uint64_t, trading_db::ShortTick> ticks;
        const size_t price_scale = 5;
        trading_db::QdbCompactDataset dataset;

        const uint64_t timestamp_start = ztime::get_timestamp(1,1,2021,0,0,0);

        for(uint64_t i = timestamp_start; i < (timestamp_start + ztime::SECONDS_IN_HOUR); i += 10) {
            const uint64_t t = i * 1000;
            const double bid = get_price(t);
            const double ask = bid + 1.1;
            ticks[t] = trading_db::ShortTick(bid, ask);
        }

        dataset.write_ticks(ticks, price_scale, timestamp_start);

        std::map<uint64_t, trading_db::ShortTick> r_ticks;
        size_t r_price_scale = 0;

        dataset.read_ticks(r_ticks, r_price_scale, timestamp_start);

        for (auto &tick : r_ticks) {
            const double bid = get_price(tick.first);
            const double ask = bid + 1.1;
            const double eps = 0.000001;
            if (!cmp_equal(bid, tick.second.bid, eps)) std::cout << "tick bid error" << std::endl;
            if (!cmp_equal(ask, tick.second.ask, eps)) std::cout << "tick ask error" << std::endl;
        }

        std::cout << "ticks size: " << ticks.size() << std::endl;
        std::cout << "r_ticks size: " << r_ticks.size() << std::endl;

        auto &data = dataset.get_data();
        std::cout << "data size: " << data.size() << std::endl;
    }

    return 0;
}
