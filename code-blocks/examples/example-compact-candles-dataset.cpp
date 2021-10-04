#include <iostream>
#include "../../include/trading-db/utility/compact_candles_dataset.hpp"

int main() {
    std::cout << "Hello world!" << std::endl;

    // проверка работы датасета
    {
        std::vector<trading_db::utility::Candle> candles;
        const size_t price_scale = 5;
        const size_t volume_scale = 5;
        trading_db::utility::CompactCandlesDataset5x1440 dataset;

        for(size_t i = 0; i < 1440; i += 1) {
            trading_db::utility::Candle candle;
            candle.open = 100.2 + 0.1 * i;
            candle.high = 100.3 + 0.1 * i;
            candle.low = 100.0 + 0.1 * i;
            candle.close = 100.1 + 0.1 * i;
            candle.volume = 100 + 0.1 * i;
            candle.timestamp = ztime::get_timestamp(1,1,2021,0,0,0) + i * 60;
            candles.push_back(candle);
        }

        dataset.write_sequence(candles, price_scale, volume_scale, ztime::get_timestamp(1,1,2021,0,0,0));

        std::vector<trading_db::utility::Candle> r_candles;
        size_t r_price_scale = 0;
        size_t r_volume_scale = 0;
        dataset.read_sequence(r_candles, r_price_scale, r_volume_scale, ztime::get_timestamp(1,1,2021,0,0,0));

        for(size_t i = 0; i < r_candles.size(); ++i) {
            std::cout << "c " << r_candles[i].close << " t " << ztime::get_str_date_time(r_candles[i].timestamp) << std::endl;
        }
    }
    {
        std::map<uint64_t, trading_db::utility::Candle> candles;
        const size_t price_scale = 5;
        const size_t volume_scale = 5;
        trading_db::utility::CompactCandlesDataset5x1440 dataset;

        for(size_t i = 0; i < 1440; i += 1) {
            trading_db::utility::Candle candle;
            candle.open = 100.2 + 0.1 * i;
            candle.high = 100.3 + 0.1 * i;
            candle.low = 100.0 + 0.1 * i;
            candle.close = 100.1 + 0.1 * i;
            candle.volume = 100 + 0.1 * i;
            candle.timestamp = ztime::get_timestamp(1,1,2021,0,0,0) + i * 60;
            candles[candle.timestamp] = candle;
        }

        dataset.write_map(candles, price_scale, volume_scale, ztime::get_timestamp(1,1,2021,0,0,0));

        /*
        std::map<uint64_t, trading_db::utility::Candle> r_candles;
        size_t r_price_scale = 0;
        size_t r_volume_scale = 0;
        dataset.read_map(r_candles, r_price_scale, r_volume_scale, ztime::get_timestamp(1,1,2021,0,0,0));

        for(auto &c : r_candles) {
            std::cout << "c " << c.second.close << " t " << ztime::get_str_date_time(c.second.timestamp) << std::endl;
        }
        */
    }

    return 0;
}
