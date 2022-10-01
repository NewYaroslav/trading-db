#include <iostream>
#include "../../include/trading-db/qdb.hpp"
#include "../../include/trading-db/parts/qdb-csv.hpp"

inline bool cmp_equal(const double x, const double y, const double eps) noexcept {
    return std::fabs(x - y) < eps;
}

int main() {
    std::cout << "start test storage db!" << std::endl;

    // настраиваем БД котировок
    const std::string path = "storage//AUDUSD-test.db";

    trading_db::QDB qdb;
    std::cout << "open status: " << qdb.open(path) << std::endl;

    // очищаем БД
    std::cout << "remove all: " << qdb.remove_all() << std::endl;

    // настраиваем CSV файл
    trading_db::QdbCsv csv;
    csv.config.time_zone = - 3* ztime::SECONDS_IN_HOUR;
    csv.config.type = trading_db::QdbCsvType::MT5_CSV_TICKS_FILE;
    //csv.config.file_name = "dataset//AUDUSD-test.csv"; // Это файл тиков!
    csv.config.file_name = "D:\\_repoz_trading\\mega-connector\\storage\\alpary-mt5\\GBPJPY.csv";

    // настраиваем данные БД
    qdb.set_info_int(trading_db::QDB::METADATA_TYPE::SYMBOL_DIGITS, 5);
    qdb.set_info_str(trading_db::QDB::METADATA_TYPE::SYMBOL_NAME, "AUDUSD");

    // данные по тикам
    size_t counter_csv_ticks = 0;
    uint64_t min_tick_time = 1000 * ztime::get_timestamp(1,1,2100,0,0,0), max_tick_time = 0;

    uint64_t last_day_time = 0;
    // настраиваем обратные вызовы
    csv.on_tick = [&](const trading_db::Tick &tick) {
        qdb.write_tick(tick);

        min_tick_time = std::min(min_tick_time, tick.timestamp_ms);
        max_tick_time = std::max(max_tick_time, tick.timestamp_ms);

        const uint64_t day_time = ztime::get_first_timestamp_day(tick.timestamp_ms/1000);
        if (last_day_time != day_time) {
            std::cout << ztime::get_str_date(day_time) << std::endl;
            last_day_time = day_time;
        }

        ///std::cout << ztime::get_str_date_time_ms(tick.timestamp_ms/1000.0) << std::endl;

        ++counter_csv_ticks;
    };

    csv.on_candle = [&](const trading_db::Candle &candle) {
        qdb.write_candle(candle);
    };

    // начинаем запись данных в БД
    std::cout << "start write" << std::endl;
    qdb.start_write();
    csv.read();
    std::cout << "stop write: " << qdb.stop_write() << std::endl;

    std::cout << "tick date: "
        << ztime::get_str_date_time_ms((double)min_tick_time/1000.0)
        << " -> "
        << ztime::get_str_date_time_ms((double)max_tick_time/1000.0)
        << std::endl;

    std::system("pause");

    // читаем данные из БД
    /*
    size_t counter_db_ticks = 0;
    uint64_t last_tick_date = 0;
    for (uint64_t t = min_tick_time; t <= max_tick_time; t++) {
        trading_db::Tick tick;
        if (qdb.get_tick_ms(tick, t)) {
            if (last_tick_date != tick.timestamp_ms) {
                std::cout << "bid " << tick.bid << " ask " << tick.ask << " t " << ztime::get_str_date_time_ms((double)tick.timestamp_ms/1000.0)  << std::endl;
                last_tick_date = tick.timestamp_ms;
                ++counter_db_ticks;
            }
        }
    }

    std::cout << "counter db ticks: " << counter_db_ticks << std::endl;
    std::cout << "counter csv ticks: " << counter_csv_ticks << std::endl;
    */

    // проводим тестирование БД

    trading_db::QdbCsv csv_test = csv;

    // настраиваем обратные вызовы
    csv_test.on_tick = [&](const trading_db::Tick &tick) {
        trading_db::Tick db_tick;
        if (qdb.get_tick_ms(db_tick, tick.timestamp_ms)) {
            const double eps = 0.000001;
            if (!cmp_equal(tick.bid, db_tick.bid, eps)) {
                std::cout << "error, db bid " << db_tick.bid << " csv bid " << tick.bid << " t " << ztime::get_str_date_time_ms((double)tick.timestamp_ms/1000.0)  << std::endl;
            }
            if (!cmp_equal(tick.ask, db_tick.ask, eps)) {
                std::cout << "error, db ask " << db_tick.ask << " csv ask " << tick.ask << " t " << ztime::get_str_date_time_ms((double)tick.timestamp_ms/1000.0)  << std::endl;
            }
        } else {
            std::cout << "error get_tick_ms, t " << ztime::get_str_date_time_ms((double)tick.timestamp_ms/1000.0)  << std::endl;
        }
    };

    csv_test.on_candle = [&](const trading_db::Candle &candle) {

    };

    csv_test.read();

    return 0;
}
