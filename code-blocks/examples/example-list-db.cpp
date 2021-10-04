#include <iostream>
#include <trading-db/list-database.hpp>

int main() {
    std::cout << "Hello world!" << std::endl;
    const std::string path("storage/example-list-db.db");
    std::cout << "#test-1" << std::endl << std::endl;
    {
        trading_db::ListDatabase list_db(path);

        std::cout << "#remove_all" << std::endl;
        std::cout << list_db.remove_all() << std::endl;

        std::cout << "#set_value" << std::endl;
        std::cout << list_db.set_value("first") << std::endl;

        std::cout << "#set_value" << std::endl;
        std::cout << list_db.set_value("second") << std::endl;

        std::cout << "#get_all_items" << std::endl;
        std::vector<trading_db::ListDatabase::Item> items = list_db.get_all_items<std::vector<trading_db::ListDatabase::Item>>();
        for (auto item : items) {
            std::cout << item.key << " " << item.value << std::endl;
        }

        std::cout << "#end" << std::endl;
    }
    std::cout << "#test-2" << std::endl << std::endl;
    {
        trading_db::ListDatabase list_db(path);

        std::cout << "#get_all_items" << std::endl;
        std::vector<trading_db::ListDatabase::Item> items = list_db.get_all_items<std::vector<trading_db::ListDatabase::Item>>();
        for (auto item : items) {
            std::cout << item.key << " " << item.value << std::endl;
        }
        std::cout << "#set_value" << std::endl;
        std::cout << list_db.set_item(2, "test-2") << std::endl;

        std::cout << "#set_item" << std::endl;
        trading_db::ListDatabase::Item item;
        item.value = "test-?";
        std::cout << list_db.set_item(item) << std::endl;
        std::cout << "key " << item.key << std::endl;

        std::cout << "#get_all_items" << std::endl;
        items = list_db.get_all_items<std::vector<trading_db::ListDatabase::Item>>();
        for (auto item : items) {
            std::cout << item.key << " " << item.value << std::endl;
        }

        std::cout << "#remove_value" << std::endl;
        std::cout << list_db.remove_value(2) << std::endl;

        std::cout << "#get_all_items" << std::endl;
        items = list_db.get_all_items<std::vector<trading_db::ListDatabase::Item>>();
        for (auto item : items) {
            std::cout << item.key << " " << item.value << std::endl;
        }

        std::cout << "#end" << std::endl;
    }
    std::cout << "#test-3" << std::endl << std::endl;
    {
        trading_db::ListDatabase list_db(path);

        std::cout << "#get_all_items" << std::endl;
        std::vector<trading_db::ListDatabase::Item> items = list_db.get_all_items<std::vector<trading_db::ListDatabase::Item>>();
        for (auto item : items) {
            std::cout << item.key << " " << item.value << std::endl;
        }

        std::cout << "#set_items" << std::endl;
        items.emplace_back(3, "test-3-1");
        items.emplace_back(3, "test-3-2");
        items.emplace_back(3, "test-3-3");
        items.emplace_back(3, "test-3-4");
        items.emplace_back(5, "test-3-5");
        items.front().value = "front";
        list_db.set_items(items);

        std::cout << "#get_all_items" << std::endl;
        std::vector<trading_db::ListDatabase::Item> items2 = list_db.get_all_items<std::vector<trading_db::ListDatabase::Item>>();
        for (auto item : items2) {
            std::cout << item.key << " " << item.value << std::endl;
        }
        std::cout << "#end" << std::endl;
    }
    std::cout << "#test-4" << std::endl << std::endl;
    {
        trading_db::ListDatabase list_db(path);

        std::cout << "#get_map_all_items" << std::endl;
        std::map<int64_t, std::string> items = list_db.get_map_all_items();
        for (auto item : items) {
            std::cout << item.first << " " << item.second << std::endl;
        }

        std::cout << "#set_items" << std::endl;
        items[4] = "test-4";
        items[6] = "items-6";
        list_db.set_map_items(items);

        std::cout << "#get_map_all_items" << std::endl;
        std::map<int64_t, std::string> items2 = list_db.get_map_all_items();
        for (auto item : items2) {
            std::cout << item.first << " " << item.second << std::endl;
        }

        std::cout << "#get_all_values" << std::endl;
        std::vector<std::string> values = list_db.get_all_values<std::vector<std::string>>();
        for (auto value : values) {
            std::cout << value << std::endl;
        }

        std::cout << "#end" << std::endl;
    }
    std::cout << "#exit" << std::endl;
    return 0;
}
