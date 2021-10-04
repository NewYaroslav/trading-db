#include <iostream>
#include <trading-db/key-value-database.hpp>

int main() {
    std::cout << "Hello world!" << std::endl;
    const std::string path("storage/example-key-value-db.db");
    std::cout << "#test-1" << std::endl << std::endl;
    {
        trading_db::KeyValueDatabase key_value_db(path);

        std::cout << "#remove_all" << std::endl;
        std::cout << key_value_db.remove_all() << std::endl;

        std::cout << "#set_pair" << std::endl;
        std::cout << key_value_db.set_pair("1", "value-1") << std::endl;

        std::cout << "#set_pair" << std::endl;
        std::cout << key_value_db.set_pair("2", "value-2") << std::endl;

        std::cout << "#set_pair" << std::endl;
        std::cout << key_value_db.set_pair("3", "value-3") << std::endl;

        std::cout << "#set_pair" << std::endl;
        std::cout << key_value_db.set_pair("3", "value-4") << std::endl;

        std::cout << "#get_all_pairs" << std::endl;
        std::vector<trading_db::KeyValueDatabase::KeyValue> pairs = key_value_db.get_all_pairs<std::vector<trading_db::KeyValueDatabase::KeyValue>>();
        for (auto pair : pairs) {
            std::cout << pair.key << " " << pair.value << std::endl;
        }

        std::cout << "#get_map_all_pairs" << std::endl;
        std::map<std::string, std::string> map_pairs = key_value_db.get_map_all_pairs();
        for (auto &p : map_pairs) {
            std::cout << p.first << " " << p.second << std::endl;
        }

        std::cout << "#set_map_pairs" << std::endl;
        map_pairs["key-2021"] = "hello-2021";
        std::cout << key_value_db.set_map_pairs(map_pairs) << std::endl;

        std::cout << "#get_map_all_pairs" << std::endl;
        std::map<std::string, std::string> map_pairs2 = key_value_db.get_map_all_pairs();
        for (auto &p : map_pairs2) {
            std::cout << p.first << " " << p.second << std::endl;
        }

        std::cout << "#end" << std::endl;
    }
    std::cout << "#test-2" << std::endl << std::endl;
    {
        trading_db::KeyValueDatabase key_value_db(path);

        std::cout << "#get_all_pairs" << std::endl;
        std::vector<trading_db::KeyValueDatabase::KeyValue> pairs = key_value_db.get_all_pairs<std::vector<trading_db::KeyValueDatabase::KeyValue>>();
        for (auto pair : pairs) {
            std::cout << pair.key << " " << pair.value << std::endl;
        }

        std::cout << "#set_pair_int64_value" << std::endl;
        std::cout << key_value_db.set_pair_int64_value("key6", 124567) << std::endl;

        std::cout << "#get_all_pairs" << std::endl;
        std::vector<trading_db::KeyValueDatabase::KeyValue> pairs2 = key_value_db.get_all_pairs<std::vector<trading_db::KeyValueDatabase::KeyValue>>();
        for (auto pair : pairs2) {
            std::cout << pair.key << " " << pair.value << std::endl;
        }

        std::cout << "#get_int64_value" << std::endl;
        std::cout << key_value_db.get_int64_value("key6") << std::endl;

        std::cout << "#end" << std::endl;
    }
    std::cout << "#test-3" << std::endl << std::endl;
    {
        trading_db::KeyValueDatabase key_value_db(path);

        std::cout << "#remove_values" << std::endl;
        std::vector<std::string> keys = {"key6", "1", "11"};
        key_value_db.remove_values(keys);

        std::cout << "#get_all_pairs" << std::endl;
        std::vector<trading_db::KeyValueDatabase::KeyValue> pairs = key_value_db.get_all_pairs<std::vector<trading_db::KeyValueDatabase::KeyValue>>();
        for (auto pair : pairs) {
            std::cout << pair.key << " " << pair.value << std::endl;
        }

        std::cout << "#remove_pairs" << std::endl;
        key_value_db.remove_pairs(pairs);

        std::cout << "#get_all_pairs" << std::endl;
        std::vector<trading_db::KeyValueDatabase::KeyValue> pair2 = key_value_db.get_all_pairs<std::vector<trading_db::KeyValueDatabase::KeyValue>>();
        for (auto pair : pair2) {
            std::cout << pair.key << " " << pair.value << std::endl;
        }

        std::cout << "#end" << std::endl;
    }
    std::cout << "#exit" << std::endl;
    return 0;
}
