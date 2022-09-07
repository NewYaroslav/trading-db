#include <iostream>
#include "../../include/trading-db/parts/qdb-storage.hpp"

int main() {
    std::cout << "start test storage db!" << std::endl;

    const std::string path = "test.db";

    std::cout << "test #0" << std::endl;
    {
        trading_db::QdbStorage storage;

        std::cout << "open status: " << storage.open(path) << std::endl;

        std::cout << "SYMBOL_NAME: " << storage.get_info_str(trading_db::QdbStorage::METADATA_TYPE::SYMBOL_NAME) << std::endl;
        std::cout << "set_info_str: " << storage.set_info_str(trading_db::QdbStorage::METADATA_TYPE::SYMBOL_NAME, "EURUSD") << std::endl;
        std::cout << "SYMBOL_NAME: " << storage.get_info_str(trading_db::QdbStorage::METADATA_TYPE::SYMBOL_NAME) << std::endl;

        std::cout << "SYMBOL_DATA_FEED_SOURCE: " << storage.get_info_str(trading_db::QdbStorage::METADATA_TYPE::SYMBOL_DATA_FEED_SOURCE) << std::endl;
        std::cout << "set_info_str: " << storage.set_info_str(trading_db::QdbStorage::METADATA_TYPE::SYMBOL_DATA_FEED_SOURCE, "Server") << std::endl;
        std::cout << "SYMBOL_DATA_FEED_SOURCE: " << storage.get_info_str(trading_db::QdbStorage::METADATA_TYPE::SYMBOL_DATA_FEED_SOURCE) << std::endl;

        std::cout << "SYMBOL_DIGITS: " << storage.get_info_int(trading_db::QdbStorage::METADATA_TYPE::SYMBOL_DIGITS) << std::endl;
        std::cout << "set_info_int: " << storage.set_info_int(trading_db::QdbStorage::METADATA_TYPE::SYMBOL_DIGITS, 123) << std::endl;
        std::cout << "SYMBOL_DIGITS: " << storage.get_info_int(trading_db::QdbStorage::METADATA_TYPE::SYMBOL_DIGITS) << std::endl;
    }

    std::cout << "test #1" << std::endl;
    {
        trading_db::QdbStorage storage;

        std::cout << "open status: " << storage.open(path) << std::endl;

        {
            std::map<uint64_t, std::vector<uint8_t>> data;
            data[0] = {1,2,3};
            data[1] = {4,5,6};
            data[2] = {7,8,9};
            std::cout << "write status: " << storage.write_candles(data) << std::endl;
        }

        {
            std::vector<uint8_t> data;
            std::cout << "read_candles: " << storage.read_candles(data, 0) << std::endl;
            for (auto &item : data) std::cout << (int)item << std::endl;
        }

        {
            std::vector<uint8_t> data;
            std::cout << "read_candles: " << storage.read_candles(data, 1) << std::endl;
            for (auto &item : data) std::cout << (int)item << std::endl;
        }

        {
            std::vector<uint8_t> data;
            std::cout << "read_candles: " << storage.read_candles(data, 2) << std::endl;
            for (auto &item : data) std::cout << (int)item << std::endl;
        }
    }

    std::cout << "test #2" << std::endl;
    //
    {
        trading_db::QdbStorage storage;

        std::cout << "open status: " << storage.open(path) << std::endl;

        {
            std::map<uint64_t, std::vector<uint8_t>> data;
            data[0] = {1,2,3, 10, 11, 12};
            std::cout << "write status: " << storage.write_candles(data) << std::endl;
        }

        {
            std::vector<uint8_t> data;
            std::cout << "read_candles: " << storage.read_candles(data, 0) << std::endl;
            for (auto &item : data) std::cout << (int)item << std::endl;
        }

        {
            std::vector<uint8_t> data;
            std::cout << "read_candles: " << storage.read_candles(data, 1) << std::endl;
            for (auto &item : data) std::cout << (int)item << std::endl;
        }

        {
            std::vector<uint8_t> data;
            std::cout << "read_candles: " << storage.read_candles(data, 2) << std::endl;
            for (auto &item : data) std::cout << (int)item << std::endl;
        }
    }

    std::cout << "test #3" << std::endl;
    {
        trading_db::QdbStorage storage;

        std::cout << "open status: " << storage.open(path) << std::endl;

        {
            std::map<uint64_t, std::vector<uint8_t>> data;
            data[0] = {11,12,13};
            data[1] = {14,15,16};
            data[2] = {17,18,19};
            std::cout << "write status: " << storage.write_ticks(data) << std::endl;
        }

        {
            std::vector<uint8_t> data;
            std::cout << "read_ticks: " << storage.read_ticks(data, 0) << std::endl;
            for (auto &item : data) std::cout << (int)item << std::endl;
        }

        {
            std::vector<uint8_t> data;
            std::cout << "read_ticks: " << storage.read_ticks(data, 1) << std::endl;
            for (auto &item : data) std::cout << (int)item << std::endl;
        }

        {
            std::vector<uint8_t> data;
            std::cout << "read_ticks: " << storage.read_ticks(data, 2) << std::endl;
            for (auto &item : data) std::cout << (int)item << std::endl;
        }
    }

    std::cout << "test #4" << std::endl;
    {
        trading_db::QdbStorage storage;

        std::cout << "open status: " << storage.open(path) << std::endl;

        {
            std::map<uint64_t, std::vector<uint8_t>> data;
            data[0] = {11,12,13, 110, 111, 112};
            std::cout << "write status: " << storage.write_ticks(data) << std::endl;
        }

        {
            std::vector<uint8_t> data;
            std::cout << "read_ticks: " << storage.read_ticks(data, 0) << std::endl;
            for (auto &item : data) std::cout << (int)item << std::endl;
        }

        {
            std::vector<uint8_t> data;
            std::cout << "read_ticks: " << storage.read_ticks(data, 1) << std::endl;
            for (auto &item : data) std::cout << (int)item << std::endl;
        }

        {
            std::vector<uint8_t> data;
            std::cout << "read_ticks: " << storage.read_ticks(data, 2) << std::endl;
            for (auto &item : data) std::cout << (int)item << std::endl;
        }
    }

    std::cout << "test #5" << std::endl;
    {
        trading_db::QdbStorage storage;

        std::cout << "open status: " << storage.open(path) << std::endl;
        storage.remove_all();

        //
        {
            std::vector<uint8_t> data;
            std::cout << "read_candles: " << storage.read_candles(data, 0) << std::endl;
            std::cout << "data size: " << data.size() << std::endl;
        }
        //
        {
            std::vector<uint8_t> data;
            std::cout << "read_candles: " << storage.read_candles(data, 1) << std::endl;
            std::cout << "data size: " << data.size() << std::endl;
        }
        //
        {
            std::vector<uint8_t> data;
            std::cout << "read_candles: " << storage.read_candles(data, 2) << std::endl;
            std::cout << "data size: " << data.size() << std::endl;
        }
        //
        {
            std::vector<uint8_t> data;
            std::cout << "read_ticks: " << storage.read_ticks(data, 0) << std::endl;
            std::cout << "data size: " << data.size() << std::endl;
        }
        //
        {
            std::vector<uint8_t> data;
            std::cout << "read_ticks: " << storage.read_ticks(data, 1) << std::endl;
            std::cout << "data size: " << data.size() << std::endl;
        }
        //
        {
            std::vector<uint8_t> data;
            std::cout << "read_ticks: " << storage.read_ticks(data, 2) << std::endl;
            std::cout << "data size: " << data.size() << std::endl;
        }
    }
    return 0;
}
