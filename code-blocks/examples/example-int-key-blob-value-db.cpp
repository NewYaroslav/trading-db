#include <iostream>
#include <trading-db/int-key-blob-value-database.hpp>

int main() {
    std::cout << "Hello world!" << std::endl;
    const std::string path("storage/example-int-key-blob-value-db.db");
    std::cout << "#test-1" << std::endl << std::endl;
    {
        using database_t = trading_db::IntKeyBlobValueDatabase<std::vector<uint8_t>>;

        database_t key_value_db;
        std::cout << "#init" << std::endl;
        key_value_db.config.title = "CandleData";
        key_value_db.config.use_log = true;
        std::cout << key_value_db.init(path) << std::endl;

        std::cout << "#remove_all" << std::endl;
        std::cout << key_value_db.remove_all() << std::endl;

        std::cout << "#set_pair" << std::endl;
        std::cout << key_value_db.set_pair(1, {1,2,3}) << std::endl;

        std::cout << "#set_pair" << std::endl;
        std::cout << key_value_db.set_pair(2, {4,5,6}) << std::endl;

        std::cout << "#set_pair" << std::endl;
        std::cout << key_value_db.set_pair(3, {7,8,9}) << std::endl;

        std::cout << "#set_pair" << std::endl;
        std::cout << key_value_db.set_pair(3, {9,7,8}) << std::endl;

        std::cout << "#get_all_values" << std::endl;
        std::vector<std::vector<uint8_t>> values = key_value_db.get_all_values<std::vector<std::vector<uint8_t>>>();
        for (auto value : values) {
            std::cout << "-" << std::endl;
            for (size_t i = 0; i < value.size(); ++i) {
                std::cout << " " << (int)value[i];
            }
            std::cout << std::endl;
        }

        /*
        std::vector<uint8_t> pairs = key_value_db.get_all_values<std::vector<uint8_t>>();
        for (auto pair : pairs) {
            std::cout << pair.key;
            for (size_t i = 0; i < pair.value.size(); ++i) {
                std::cout << " " << pair.value[i];
            }
            std::cout << std::endl;
        }
        */

        /*
        std::cout << "#get_map_all_pairs" << std::endl;
        std::map<int64_t, std::vector<uint8_t>> map_pairs = key_value_db.get_map_all_pairs();
        for (auto &p : map_pairs) {
            std::cout << p.first;
            for (size_t i = 0; i < p.second.size(); ++i) {
                std::cout << " " << p.second[i];
            }
            std::cout << std::endl;
        }
        */

        std::cout << "#end" << std::endl;
    }
    std::cout << "#exit" << std::endl;
    return 0;
}
