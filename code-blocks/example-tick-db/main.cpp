#include <iostream>
#include "../../include/tick-db.hpp"
//#include <sqlite_orm/sqlite_orm.h>
#include <xtime.hpp>

int main() {
    std::cout << "Hello world!" << std::endl;
    const std::string path("test_storage.db");
    {
        std::cout << "#test 1" << std::endl;
        trading_db::TickDb tick_db("test_storage.db");
        trading_db::TickDb tick_db2("test_storage.db");
        tick_db.set_symbol("TEST");
        tick_db.set_digits(5);
        tick_db.set_hostname("my pc");
        tick_db.set_server_name("XBT");


        std::vector<trading_db::TickDb::Tick> ticks_1, ticks_2, ticks_3;
        std::cout << "write price 1" << std::endl;
        for(size_t i = 0; i < 1000; ++i) {
            const uint64_t timestamp = i * 1000;
            trading_db::TickDb::Tick tick;
            tick.bid = 0.3 + (double)i;
            tick.ask = 0.5 + (double)i;
            tick.timestamp = timestamp;
            tick.server_timestamp = timestamp + 1;
            tick_db.write(tick);
            ticks_1.push_back(tick);
        }
        tick_db.stop_write(1000*1000);
        tick_db.wait();

        std::cout << "check price 1" << std::endl;
        for(size_t i = 0; i < 1000; ++i) {
            const uint64_t timestamp = i * 1000;
            trading_db::TickDb::Tick tick = tick_db.get(timestamp);
            if (tick.timestamp != ticks_1[i].timestamp) {
                std::cout << "check price error, index " << i << std::endl;
                return 0;
            }
        }

        std::cout << "write price 2" << std::endl;
        ticks_2.push_back(trading_db::TickDb::Tick());
        for(size_t i = 1002; i < 2000; ++i) {
            const uint64_t timestamp = i * 1000;
            trading_db::TickDb::Tick tick;
            tick.bid = 0.3 + (double)i;
            tick.ask = 0.5 + (double)i;
            tick.timestamp = timestamp;
            tick.server_timestamp = timestamp + 1;
            tick_db.write(tick);
            ticks_2.push_back(tick);
        }
        ticks_2.push_back(trading_db::TickDb::Tick());
        tick_db.stop_write(1999*1000);
        tick_db.wait();

        std::cout << "check price 2" << std::endl;
        for(size_t i = 1001; i <= 2000; ++i) {
            const uint64_t timestamp = i * 1000;
            const uint64_t index = i - 1001;
            trading_db::TickDb::Tick tick = tick_db.get(timestamp);
            if (tick.timestamp != ticks_2[index].timestamp) {
                std::cout << "check price error, index " << i << std::endl;
                std::cout << "get t: " << timestamp <<  " t: " << tick.timestamp << " bid " << tick.bid << " ask " << tick.ask << std::endl;
                std::cout << "src t: " << timestamp <<  " t: " << ticks_2[index].timestamp << " bid " << ticks_2[index].bid << " ask " << ticks_2[index].ask << std::endl;
                //return 0;
            }
        }

        std::cout << "write price 3" << std::endl;
        for(size_t i = 2000; i <= 3000; ++i) {
            const uint64_t timestamp = i * 1000;
            trading_db::TickDb::Tick tick;
            tick.bid = 0.3 + (double)i;
            tick.ask = 0.5 + (double)i;
            tick.timestamp = timestamp;
            tick.server_timestamp = timestamp + 1;
            tick_db.write(tick);
            ticks_3.push_back(tick);
        }
        tick_db.stop_write(3000*1000);
        tick_db.wait();

        std::cout << "check price 3" << std::endl;
        for(size_t i = 2000; i <= 3000; ++i) {
            const uint64_t timestamp = i * 1000;
            const uint64_t index = i - 2000;
            trading_db::TickDb::Tick tick = tick_db.get(timestamp);
            if (tick.timestamp != ticks_3[index].timestamp) {
                std::cout << "check price error, index " << i << std::endl;
                return 0;
            }
        }

        std::cout << "check price 2.2" << std::endl;
        for(size_t i = 1001; i <= 1999; ++i) {
            const uint64_t timestamp = i * 1000;
            const uint64_t index = i - 1001;
            trading_db::TickDb::Tick tick = tick_db.get(timestamp);
            if (tick.timestamp != ticks_2[index].timestamp) {
                std::cout << "check price error, index " << i << std::endl;

                return 0;
            }
        }

        std::cout << "check price 1.1" << std::endl;
        for(size_t i = 0; i < 1000; ++i) {
            const uint64_t timestamp = i * 1000;
            const uint64_t index = i - 0;
            trading_db::TickDb::Tick tick = tick_db.get(timestamp);
            if (tick.timestamp != ticks_1[index].timestamp) {
                std::cout << "check price error, index " << i << std::endl;

                return 0;
            }
        }

        std::cout << "get_price" << std::endl;
        for(size_t i = 999; i <= 1000; ++i) {
            const uint64_t timestamp = i * 1000;
            trading_db::TickDb::Tick tick = tick_db.get(timestamp);
            std::cout << "get t: " << timestamp <<  " t: " << tick.timestamp << " bid " << tick.bid << " ask " << tick.ask << std::endl;
        }

        std::cout << "get_price" << std::endl;
        for(size_t i = 1000; i <= 1010; ++i) {
            const uint64_t timestamp = i * 1000;;
            trading_db::TickDb::Tick tick = tick_db.get(timestamp);
            std::cout << "get t: " << timestamp <<  " t: " << tick.timestamp << " bid " << tick.bid << " ask " << tick.ask << std::endl;
        }
        std::cout << "get_symbol: " << tick_db.get_symbol() << std::endl;
        std::cout << "get_digits: " << tick_db.get_digits() << std::endl;
        std::cout << "get_hostname: " << tick_db.get_hostname() << std::endl;
        std::cout << "get_server_name: " << tick_db.get_server_name() << std::endl;
        std::cout << "#end test 1" << std::endl;
    }
    return 0;
    {
        std::cout << "#test 2" << std::endl;
        trading_db::TickDb tick_db("test_storage.db");
        std::cout << "get_price" << std::endl;
        for(size_t i = 1000; i <= 1010; ++i) {
            const uint64_t timestamp = i;
            trading_db::TickDb::Tick tick = tick_db.get(timestamp);
            std::cout << "get t: " << timestamp <<  " t: " << tick.timestamp << " bid " << tick.bid << " ask " << tick.ask << std::endl;
        }
        std::cout << "#end test 2" << std::endl;
    }
    {
        std::cout << "#test 3" << std::endl;
        trading_db::TickDb tick_db("test_storage.db");

        std::cout << "write price" << std::endl;
        for(size_t i = 1010; i < 1000000; ++i) {
            const uint64_t timestamp = i;
            trading_db::TickDb::Tick tick;
            tick.bid = 0.3 + (double)i;
            tick.ask = 0.5 + (double)i;
            tick.timestamp = timestamp;
            tick.server_timestamp = timestamp + 1;
            tick_db.write(tick);
        }

        tick_db.stop_write(1000000);

        std::cout << "get_price" << std::endl;
        for(size_t i = 1000000 - 110; i <= 1000000 - 100; ++i) {
            const uint64_t timestamp = i;
            trading_db::TickDb::Tick tick = tick_db.get(timestamp);
            std::cout << "get t: " << timestamp <<  " t: " << tick.timestamp << " bid " << tick.bid << " ask " << tick.ask << std::endl;
        }

        // std::cout << "get_symbol: " << tick_db.get_symbol() << std::endl;
    }
    {
        std::cout << "#test 4" << std::endl;
        trading_db::TickDb tick_db("test_storage.db");

        std::cout << "get_price" << std::endl;
        for(size_t i = 1000000 - 110; i <= 1000000 - 100; ++i) {
            const uint64_t timestamp = i;
            trading_db::TickDb::Tick tick = tick_db.get(timestamp);
            std::cout << "get t: " << timestamp <<  " t: " << tick.timestamp << " bid " << tick.bid << " ask " << tick.ask << std::endl;
        }

        std::cout << "get_price" << std::endl;
        for(size_t i = 1000; i <= 1010; ++i) {
            const uint64_t timestamp = i;
            trading_db::TickDb::Tick tick = tick_db.get(timestamp);
            std::cout << "get t: " << timestamp <<  " t: " << tick.timestamp << " bid " << tick.bid << " ask " << tick.ask << std::endl;
        }

        std::cout << "get_price" << std::endl;
        for(size_t i = 10; i <= 20; ++i) {
            const uint64_t timestamp = i;
            trading_db::TickDb::Tick tick = tick_db.get(timestamp);
            std::cout << "get t: " << timestamp <<  " t: " << tick.timestamp << " bid " << tick.bid << " ask " << tick.ask << std::endl;
        }

        // std::cout << "get_symbol: " << tick_db.get_symbol() << std::endl;
    }
    return 0;
}
