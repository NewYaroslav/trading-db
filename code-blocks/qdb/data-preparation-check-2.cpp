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

    const std::string path_traning_data = "..//..//dataset//training_data//ticks//";
    const int64_t time_zone = -3 * ztime::SECONDS_IN_HOUR;

    std::vector<FileData> candles_csv = {
        FileData("..//..//dataset//ticks//BTCUSD.csv", "BTCUSD", 2, 0),
        FileData("..//..//dataset//ticks//CADJPY.csv", "CADJPY", 3, 0),
        FileData("..//..//dataset//ticks//ETHUSD.csv", "ETHUSD", 2, 0),
        FileData("..//..//dataset//ticks//EURUSD.csv", "EURUSD", 5, 0),
        FileData("..//..//dataset//ticks//NAS100.csv", "NAS100", 1, 0),
        FileData("..//..//dataset//ticks//NZDUSD.csv", "NZDUSD", 5, 0),
        FileData("..//..//dataset//ticks//STOXX50.csv", "STOXX50", 1, 0),
        FileData("..//..//dataset//ticks//XAUUSD.csv", "XAUUSD", 5, 0),
        FileData("..//..//dataset//ticks//XPRUSD.csv", "XPRUSD", 5, 0),
    };

    size_t test_part_counter = 0;
    uint64_t sum_size = 0;

    // делаем перебор всех файлов
    for (auto file_data : candles_csv) {
        trading_db::QdbCsv csv;
        csv.config.file_name = file_data.file_name;
        csv.config.time_zone = time_zone;
        csv.config.type = trading_db::QdbCsvType::MT5_CSV_TICKS_FILE;

        trading_db::QdbWriterPriceBuffer writer_buffer;

        writer_buffer.on_ticks = [&](const std::map<uint64_t, trading_db::ShortTick> &ticks, const uint64_t t) {

            std::vector<uint8_t> data;
            // сжимаем данные
            data_preparation.config.price_scale = file_data.price_scale;
            if (!data_preparation.compress_ticks(t, ticks, data)) {
                std::cout << "error compress_ticks!" << std::endl;
                std::system("pause");
            }
            sum_size += data.size();
            // производим декомпрессию данных
            std::map<uint64_t, trading_db::ShortTick> decompressed_ticks;
            if (!data_preparation.decompress_ticks(t, data, decompressed_ticks)) {
                std::cout << "error decompress_ticks! " << std::endl;
                std::cout << "data size: " << data.size() << std::endl;
                std::system("pause");
            }

            const double eps = 1.0 / std::pow(10, file_data.price_scale);

            if (decompressed_ticks.empty() && !ticks.empty()) {
                std::cout << "error decompressed_ticks! " << std::endl;
                std::cout << "decompressed ticks size: " << decompressed_ticks.size() << std::endl;
                std::system("pause");
            }
            for (auto &item : decompressed_ticks) {
                auto it = ticks.find(item.first);
                if (it == ticks.end()) {
                    std::cout << "no tick! " << ztime::get_str_date_time(item.first) << std::endl;
                    std::system("pause");
                    continue;
                }

                if (!cmp_equal(it->second.bid, item.second.bid, eps)) {
                    std::cout << "error cmp!"
                        << " dst bid: " << item.second.bid
                        << " src bid: " << it->second.bid
                        << " m: " << ztime::get_str_date_time(item.first)
                        << " p: " << test_part_counter
                        << std::endl;
                    std::system("pause");
                }
                if (!cmp_equal(it->second.ask, item.second.ask, eps)) {
                    std::cout << "error cmp!"
                        << " dst ask: " << item.second.ask
                        << " src ask: " << it->second.ask
                        << " m: " << ztime::get_str_date_time(item.first)
                        << " p: " << test_part_counter
                        << std::endl;
                    std::system("pause");
                }
            }

            ++test_part_counter;
        };

        csv.on_tick = [&](const trading_db::Tick &tick) {
            writer_buffer.write(tick);
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
