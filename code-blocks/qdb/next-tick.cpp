#include <iostream>
#include "../../include/trading-db/qdb.hpp"

int main() {
    const std::string path = "storage//AUDUSD-test.db";

    trading_db::QDB qdb;
    std::cout << "open status: " << qdb.open(path) << std::endl;

    uint64_t t_min = 0, t_max = 0;
    qdb.get_min_max_date(true, t_min, t_max);
    std::cout << "t_min: " << ztime::get_str_date_time(t_min) << std::endl;
    std::cout << "t_max: " << ztime::get_str_date_time(t_max) << std::endl;

    using METADATA_TYPE = trading_db::QdbStorage::METADATA_TYPE;
    std::cout << "'symbol' = " << qdb.get_info_str(METADATA_TYPE::SYMBOL_NAME) << std::endl;
    std::cout << "'digits' = " << qdb.get_info_int(METADATA_TYPE::SYMBOL_DIGITS) << std::endl;
    std::cout << "'digits' = " << qdb.config.digits << std::endl;

    std::system("pause");
    {
        uint64_t counter = 0;
        uint64_t t_ms = t_min * ztime::MILLISECONDS_IN_SECOND - 1;
        uint64_t t_ms_max = t_max * ztime::MILLISECONDS_IN_SECOND;
        while (true) {
            trading_db::Tick tick;
            if (!qdb.get_next_tick_ms(tick, t_ms, t_ms_max)) break;
            t_ms = tick.timestamp_ms;
            if (counter > 1000) {
                counter = 0;
                std::cout << "tick: " << ztime::get_str_date_time_ms((double)tick.timestamp_ms / 1000.0) << std::endl;
            } else {
                counter++;
            }
        }
        std::cout << "end" << std::endl;
        std::cout << "t_min: " << ztime::get_str_date_time(t_min) << std::endl;
        std::cout << "t_max: " << ztime::get_str_date_time(t_max) << std::endl;
    }
    std::system("pause");
    {
        uint64_t counter = 0;
        uint64_t t_ms = t_min * ztime::MILLISECONDS_IN_SECOND - 1;
        uint64_t t_ms_max = t_max * ztime::MILLISECONDS_IN_SECOND;
        while (true) {
            trading_db::Tick tick;
            if (!qdb.get_next_tick_ms(tick, t_ms, t_ms_max)) break;
            t_ms = tick.timestamp_ms;
            if (counter > 1000) {
                counter = 0;
                std::cout << "tick: " << ztime::get_str_date_time_ms((double)tick.timestamp_ms / 1000.0) << std::endl;
            } else {
                counter++;
            }
        }
        std::cout << "end" << std::endl;
        std::cout << "t_min: " << ztime::get_str_date_time(t_min) << std::endl;
        std::cout << "t_max: " << ztime::get_str_date_time(t_max) << std::endl;
    }
    std::system("pause");

    {
        uint64_t counter = 0;
        uint64_t t_ms = t_min * ztime::MILLISECONDS_IN_SECOND - 1;
        uint64_t t_ms_max = t_max * ztime::MILLISECONDS_IN_SECOND;
        while (true) {
            trading_db::Tick tick;
            if (!qdb.get_next_tick_ms(tick, t_ms, t_ms_max)) break;
            t_ms = tick.timestamp_ms;
            trading_db::Tick tick_r;
            if (!qdb.get_tick_ms(tick_r, tick.timestamp_ms)) {
                std::cout << "error #1, date " << ztime::get_str_date_time_ms((double)tick.timestamp_ms / 1000.0) << std::endl;
                break;
            }
            if (tick.timestamp_ms != tick_r.timestamp_ms) {
                std::cout << "error #2, date " << ztime::get_str_date_time_ms((double)tick.timestamp_ms / 1000.0) << std::endl;
                break;
            }
            if (counter > 1000) {
                counter = 0;
                std::cout << "tick: " << ztime::get_str_date_time_ms((double)tick.timestamp_ms / 1000.0) << std::endl;
            } else {
                counter++;
            }
        }
        std::cout << "end" << std::endl;
        std::cout << "t_min: " << ztime::get_str_date_time(t_min) << std::endl;
        std::cout << "t_max: " << ztime::get_str_date_time(t_max) << std::endl;
    }
    std::system("pause");
}
