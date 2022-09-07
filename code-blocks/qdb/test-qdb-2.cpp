#include <iostream>
#include "../../include/trading-db/qdb.hpp"

inline bool cmp_equal(const double x, const double y, const double eps) noexcept {
    return std::fabs(x - y) < eps;
}

int main() {
    std::cout << "start test storage db!" << std::endl;

    // настраиваем БД котировок
    const std::string path = "storage//AUDUSD-test-2.db";

    trading_db::QDB qdb;
    std::cout << "open status: " << qdb.open(path) << std::endl;

    // очищаем БД
    std::cout << "remove all: " << qdb.remove_all() << std::endl;

    // настраиваем CSV файл
    trading_db::QdbCsv csv;
    csv.config.time_zone = - 3* ztime::SECONDS_IN_HOUR;
    csv.config.type = trading_db::QdbCsvType::MT5_CSV_CANDLES_FILE;
    csv.config.file_name = "dataset//AUDUSD-test-2.csv"; // Это файл тиков!

    // настраиваем данные БД
    qdb.set_info_int(trading_db::QDB::METADATA_TYPE::SYMBOL_DIGITS, 5);
    qdb.set_info_str(trading_db::QDB::METADATA_TYPE::SYMBOL_NAME, "AUDUSD");

    // данные по барам
    uint64_t min_candle_time = 1000 * ztime::get_timestamp(1,1,2100,0,0,0), max_candle_time = 0;

    // настраиваем обратные вызовы
    csv.on_tick = [&](const trading_db::Tick &tick) {
        qdb.write_tick(tick);
    };

    csv.on_candle = [&](const trading_db::Candle &candle) {
        qdb.write_candle(candle);
        if (!candle.empty()) {
            min_candle_time = std::min(min_candle_time, candle.timestamp);
            max_candle_time = std::max(max_candle_time, candle.timestamp);
        }
    };

    // начинаем запись данных в БД
    std::cout << "start write" << std::endl;
    qdb.start_write();
    csv.read();
    std::cout << "stop write: " << qdb.stop_write() << std::endl;

    std::cout << "candles date: "
        << ztime::get_str_date_time(min_candle_time)
        << " -> "
        << ztime::get_str_date_time(max_candle_time)
        << std::endl;


    // проводим тестирование БД

    trading_db::QdbCsv csv_test = csv;

    // настраиваем обратные вызовы
    csv_test.on_tick = [&](const trading_db::Tick &tick) {

    };

    csv_test.on_candle = [&](const trading_db::Candle &candle) {
        trading_db::Candle db_candle;
        if (qdb.get_candle(db_candle, candle.timestamp)) {
            const double eps = 0.000001;
            if (!cmp_equal(db_candle.close, candle.close, eps)) {
                std::cout << "error, db c " << db_candle.close << " csv c " << candle.close << " t " << ztime::get_str_date_time(candle.timestamp)  << std::endl;
            }
            if (!cmp_equal(db_candle.low, candle.low, eps)) {
                std::cout << "error, db l " << db_candle.low << " csv l " << candle.low << " t " << ztime::get_str_date_time(candle.timestamp)  << std::endl;
            }
            if (!cmp_equal(db_candle.high, candle.high, eps)) {
                std::cout << "error, db h " << db_candle.high << " csv h " << candle.high << " t " << ztime::get_str_date_time(candle.timestamp)  << std::endl;
            }
            if (!cmp_equal(db_candle.open, candle.open, eps)) {
                std::cout << "error, db o " << db_candle.open << " csv o " << candle.open << " t " << ztime::get_str_date_time(candle.timestamp)  << std::endl;
            }
            //std::cout << "ok, db o " << db_candle.close << " csv o " << candle.close << " t " << ztime::get_str_date_time(candle.timestamp)  << std::endl;
        } else {
            std::cout << "error get_candle, t " << ztime::get_str_date_time(candle.timestamp)  << std::endl;
        }
    };

    csv_test.read();


    std::cout << "start test date: "
        << ztime::get_str_date_time(min_candle_time)
        << " -> "
        << ztime::get_str_date_time(max_candle_time)
        << std::endl;

    bool is_error = false;
    for (uint64_t t = min_candle_time; t < max_candle_time; t += ztime::SECONDS_IN_MINUTE) {
        trading_db::Candle db_candle;
        if (qdb.get_candle(db_candle, t)) {
            std::cout << "ok, o " << db_candle.open << " c " << db_candle.close << " t " << ztime::get_str_date_time(db_candle.timestamp)  << std::endl;
            is_error = false;
        } else {
            if (!is_error) std::cout << "error get_candle, t " << ztime::get_str_date_time(t)  << std::endl;
            is_error = true;
        }
    }

    return 0;
}
