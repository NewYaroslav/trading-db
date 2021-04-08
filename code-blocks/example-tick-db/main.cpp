#include <iostream>
#include <trading-db/tick-db.hpp>
//#include <sqlite_orm/sqlite_orm.h>
#include <xtime.hpp>

int main() {
    std::cout << "Hello world!" << std::endl;
    const std::string path("test_storage.db");
    {
        std::map<std::string, std::shared_ptr<trading_db::TickDb>> ticks_db;
        std::vector<std::string> symbols = {
            "AUDCAD","AUDCHF","AUDJPY","AUDNZD",
            "AUDUSD","CADCHF","CADJPY","CHFJPY",
            "EURAUD","EURCAD","EURCHF","EURGBP",
            "EURJPY","EURNZD","EURUSD","GBPAUD",
            "GBPCAD","GBPCHF","GBPJPY","GBPNZD",
            "GBPUSD","NZDCAD","NZDCHF","NZDJPY",
            "NZDUSD","USDCAD","USDCHF","USDJPY"
        };
        for(size_t i = 0; i < symbols.size(); ++i) {
            std::string path("storage/");
            path += symbols[i];
            path += ".db";
            ticks_db[symbols[i]] = std::make_shared<trading_db::TickDb>(path);
            ticks_db[symbols[i]]->set_symbol(symbols[i]);
        }

        trading_db::AsynTasks asyn_tasks;

        asyn_tasks.creat_task([&](){
            for(size_t j = 0; j < 1000000; ++j)
            for(size_t i = 0; i < symbols.size(); ++i) {
                uint64_t timestamp_ms = (uint64_t)(xtime::get_ftimestamp() * 1000.0d);
                double bid = 10.0d + i;
                double ask = 11.0d + i;
                std::cout << symbols[i] << " bid " << bid << " ask " << ask << " t: " << timestamp_ms << std::endl;
                ticks_db[symbols[i]]->write(timestamp_ms, timestamp_ms + 1, bid, ask);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });

        std::cout << "end" << std::endl;
        while(true) {};
        return 0;

    }
    if (false) {
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
        tick_db.stop_write();
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
        tick_db.stop_write();
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
        tick_db.stop_write();
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

        tick_db.stop_write();

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
