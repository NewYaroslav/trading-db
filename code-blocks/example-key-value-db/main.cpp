#include <iostream>
#include <trading-db/key-value-database.hpp>
#include <xtime.hpp>

int main() {
    std::cout << "Hello world!" << std::endl;
    const std::string path("test_storage.db");
    {
        trading_db::KeyValueDatabase kvdb(path);

        std::cout << "test #1" << std::endl;
        kvdb.set_value("1", "test#1");
        kvdb.set_value("2", "test#2");
        kvdb.set_value("3", "test#3");
        kvdb.set_value("key4", "test#4");

        std::cout << "test #2" << std::endl;
        std::vector<trading_db::KeyValueDatabase::KeyValue> temp = kvdb.get_all();
        for (size_t i = 0; i < temp.size(); ++i) {
            std::cout << "key: " << temp[i].key << " value: " << temp[i].value << std::endl;
        }

        std::cout << "test #3" << std::endl;
        kvdb.remove_all();

        std::cout << "test #4" << std::endl;
        kvdb.set_value("key5", "test#5");
        kvdb.set_int64_value("key6", 124567);

        std::cout << "test #5" << std::endl;
        temp = kvdb.get_all();
        for (size_t i = 0; i < temp.size(); ++i) {
            std::cout << "key: " << temp[i].key << " value: " << temp[i].value << std::endl;
        }

        std::cout << "test #6" << std::endl;
        std::cout << kvdb.get_int64_value("key6") << std::endl;
        // std::cout << "get_symbol: " << tick_db.get_symbol() << std::endl;
    }
    return 0;
}
