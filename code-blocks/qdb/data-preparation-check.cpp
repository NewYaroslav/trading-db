#include <iostream>
#include "trading-db/parts/qdb-csv.hpp"
#include "trading-db/parts/qdb-writer-price-buffer.hpp"
#include "trading-db/parts/qdb-data-preparation.hpp"

inline bool cmp_equal(const double x, const double y, const double eps) noexcept {
    return std::fabs(x - y) < eps;
}

class FileData {
public:
    std::string file_name;
    std::string symbol;
    size_t      price_scale     = 0;
    size_t      volume_scale    = 0;

    FileData(const std::string &fn, const std::string &s, const size_t ps, const size_t vs) :
        file_name(fn), symbol(s), price_scale(ps), volume_scale(vs) {};
};

int main() {
    std::cout << "start test" << std::endl;

    trading_db::QdbDataPreparation data_preparation;

    const std::string path_traning_data = "..//..//dataset//training_data//candles//";
    const int64_t time_zone = -3 * ztime::SECONDS_IN_HOUR;

    std::vector<FileData> candles_csv = {
        FileData("..//..//dataset//candles//BTCUSD.csv", "BTCUSD", 2, 0),
        FileData("..//..//dataset//candles//CADJPY.csv", "CADJPY", 3, 0),
        FileData("..//..//dataset//candles//ETHUSD.csv", "ETHUSD", 2, 0),
        FileData("..//..//dataset//candles//EURUSD.csv", "EURUSD", 5, 0),
        FileData("..//..//dataset//candles//NAS100.csv", "NAS100", 1, 0),
        FileData("..//..//dataset//candles//NZDUSD.csv", "NZDUSD", 5, 0),
        FileData("..//..//dataset//candles//STOXX50.csv", "STOXX50", 1, 0),
        FileData("..//..//dataset//candles//XAUUSD.csv", "XAUUSD", 5, 0),
        FileData("..//..//dataset//candles//XPRUSD.csv", "XPRUSD", 5, 0),
    };

    size_t test_part_counter = 0;
    uint64_t sum_size = 0;

    // делаем перебор всех файлов
    for (auto file_data : candles_csv) {
        trading_db::QdbCsv csv;
        csv.config.file_name = file_data.file_name;
        csv.config.time_zone = time_zone;
        csv.config.type = trading_db::QdbCsvType::MT5_CSV_CANDLES_FILE;

        trading_db::QdbWriterPriceBuffer writer_buffer;

        writer_buffer.on_candles = [&](const std::array<trading_db::Candle, ztime::MINUTES_IN_DAY> &candles, const uint64_t t) {

            std::vector<uint8_t> data;
            // сжимаем данные
            data_preparation.config.price_scale = file_data.price_scale;
            if (!data_preparation.compress_candles(candles, data)) {
                std::cout << "error compress_candles!" << std::endl;
                std::system("pause");
            }
            sum_size += data.size();
            // производим декомпрессию данных
            std::array<trading_db::Candle, ztime::MINUTES_IN_DAY> decompressed_candles;
            if (!data_preparation.decompress_candles(t, data, decompressed_candles)) {
                std::cout << "error decompress_candles! " << std::endl;
                std::cout << "data size: " << data.size() << std::endl;
                std::system("pause");
            }

            const double eps = 1.0 / std::pow(10, file_data.price_scale);

            for (size_t m = 0; m < ztime::MINUTES_IN_DAY; ++m) {
                if (!cmp_equal(decompressed_candles[m].close, candles[m].close, eps)) {
                    std::cout << "error cmp!"
                        << " dst c: " << decompressed_candles[m].close
                        << " src c: " << candles[m].close
                        << " m: " << m
                        << " p: " << test_part_counter
                        << std::endl;
                    std::system("pause");
                }
                if (!cmp_equal(decompressed_candles[m].open, candles[m].open, eps)) {
                    std::cout << "error cmp!"
                        << " dst c: " << decompressed_candles[m].close
                        << " src c: " << candles[m].close
                        << " m: " << m
                        << " p: " << test_part_counter
                        << std::endl;
                    std::system("pause");
                }
                if (!cmp_equal(decompressed_candles[m].low, candles[m].low, eps)) {
                    std::cout << "error cmp!"
                        << " dst l: " << decompressed_candles[m].low
                        << " src l: " << candles[m].low
                        << " m: " << m
                        << " p: " << test_part_counter
                        << std::endl;
                }
                if (!cmp_equal(decompressed_candles[m].high, candles[m].high, eps)) {
                    std::cout << "error cmp!"
                        << " dst h: " << decompressed_candles[m].high
                        << " src h: " << candles[m].high
                        << " m: " << m
                        << " p: " << test_part_counter
                        << std::endl;
                    std::system("pause");
                }
                if (!cmp_equal(decompressed_candles[m].volume, candles[m].volume, eps)) {
                    std::cout << "error cmp!"
                        << " dst v: " << decompressed_candles[m].volume
                        << " src v: " << candles[m].volume
                        << " m: " << m
                        << " p: " << test_part_counter
                        << std::endl;
                    std::system("pause");
                }
                if (decompressed_candles[m].timestamp != candles[m].timestamp) {
                    std::cout << "error cmp!"
                        << " dst t: " << decompressed_candles[m].timestamp
                        << " src t: " << candles[m].timestamp
                        << " m: " << m
                        << " p: " << test_part_counter
                        << std::endl;
                    std::system("pause");
                }
            }


            ++test_part_counter;
        };

        csv.on_candle = [&](const trading_db::Candle &candle) {
            writer_buffer.write(candle);
        };

        std::cout << "process start, file:  " << file_data.file_name << std::endl;
        writer_buffer.start();
        csv.read();
        writer_buffer.stop();
        std::cout << "process stop, parts: " << test_part_counter << std::endl;

        if (test_part_counter) std::cout << "avg. size: " << (sum_size/test_part_counter) << std::endl;
    }

    std::cout << "end test" << std::endl;
    return 0;
}
