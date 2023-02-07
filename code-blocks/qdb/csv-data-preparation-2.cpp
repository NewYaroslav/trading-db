#include <iostream>
#include "../../include/trading-db/tools/qdb/csv.hpp"
#include "../../include/trading-db/parts/qdb/writer-price-buffer.hpp"
#include "../../include/trading-db/parts/qdb/zstd.hpp"
#include "../../include/trading-db/parts/qdb/compact-dataset.hpp"
#include <map>

inline double get_price(const uint64_t t) noexcept {
    const uint64_t temp = t % 100 + 100;
    return (double)temp / 100.0;
}

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
    std::cout << "start data preparation!" << std::endl;

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

    size_t training_file_counter = 0;
    // делаем перебор всех файлов
    for (auto file_data : candles_csv) {
        trading_db::QdbCsv csv;
        csv.config.file_name = file_data.file_name;
        csv.config.time_zone = time_zone;
        csv.config.type = trading_db::QdbCsvType::MT5_CSV_CANDLES_FILE;

        trading_db::QdbWriterPriceBuffer writer_buffer;

        writer_buffer.on_candles = [&](const std::array<trading_db::Candle, ztime::MINUTES_IN_DAY> &candles, const uint64_t t) {
            trading_db::QdbCompactDataset dataset;
            // преобразуем в бинарные данные
            dataset.write_candles(candles, file_data.price_scale, file_data.volume_scale);

            {
                // получаем бинарные данные и записываем в файл
                auto &data = dataset.get_data();

                const std::string part_file_name = path_traning_data + "//" + file_data.symbol + "//a" + std::to_string(training_file_counter) + ".dat";
                trading_db::utils::create_directory(part_file_name, true);
                trading_db::utils::write_file(part_file_name, data.data(), data.size());
            }

            auto m_candles = candles;
            for (auto &c : m_candles) {
                c.volume = 0;
            }
            // преобразуем в бинарные данные
            dataset.write_candles(m_candles, file_data.price_scale, file_data.volume_scale);

            {
                // получаем бинарные данные и записываем в файл
                auto &data = dataset.get_data();

                const std::string part_file_name = path_traning_data + "//" + file_data.symbol + "//b" + std::to_string(training_file_counter) + ".dat";
                trading_db::utils::create_directory(part_file_name, true);
                trading_db::utils::write_file(part_file_name, data.data(), data.size());
            }

            ++training_file_counter;
        };

        csv.on_candle = [&](const trading_db::Candle &candle) {
            writer_buffer.write(candle);
        };

        std::cout << "process start, file:  " << file_data.file_name << std::endl;
        writer_buffer.start();
        csv.read();
        writer_buffer.stop();
        std::cout << "process stop, parts: " << training_file_counter << std::endl;
    }

    return 0;
}
