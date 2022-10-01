#pragma once
#ifndef TRADING_DB_QDB_CSV_HPP_INCLUDED
#define TRADING_DB_QDB_CSV_HPP_INCLUDED

#include "qdb-common.hpp"
#include <vector>
#include <cmath>
#include <fstream>
#include "ztime.hpp"

namespace trading_db {

    enum class QdbCsvType {
        MT5_CSV_TICKS_FILE,
        MT5_CSV_CANDLES_FILE,
    };

    enum QdbTimeZone {
        CET_TO_GMT,
        MSK_TO_GMT,
    };

    class QdbCsv {
    public:

        class Config {
        public:
            std::string file_name;
            QdbCsvType  type = QdbCsvType::MT5_CSV_TICKS_FILE;
            int64_t     time_zone = 0;
        } config;

        std::function<void(const Tick &tick)>        on_tick = nullptr;
        std::function<void(const Candle &candle)>    on_candle = nullptr;

    private:

        void parse_line(std::string line, std::vector<std::string> &elemets) noexcept {
            if(line.back() != '\n') line += "\n";
            std::size_t start_pos = 0;
            while(true) {
                std::size_t found_beg = line.find_first_of("\n\t ", start_pos);
                if(found_beg != std::string::npos) {
                    std::size_t len = found_beg - start_pos;
                    if(len > 0) elemets.push_back(line.substr(start_pos, len));
                    start_pos = found_beg + 1;
                } else break;
            }
        }

        void parse_line_v2(std::string line, std::vector<std::string> &elemets) noexcept {
            if (line.back() != '\t') line += "\t";
            std::size_t start_pos = 0;
            while (!false) {
                std::size_t found_beg = line.find_first_of("\t", start_pos);
                if (found_beg != std::string::npos) {
                    std::size_t len = found_beg - start_pos;
                    //if (len > 0)
                    elemets.push_back(line.substr(start_pos, len));
                    start_pos = found_beg + 1;
                } else break;
            }
        }

        inline uint64_t change_timezone(const uint64_t t_ms) noexcept {
            return (uint64_t)((int64_t)t_ms + config.time_zone * (int64_t)ztime::MILLISECONDS_IN_SECOND);
        }

        inline bool parse_mt5_ticks(std::ifstream &file) noexcept {
            // получаем заголовок файла
            std::string buffer;
            if (!std::getline(file, buffer)) return false;
            Tick tick;
            while(std::getline(file, buffer)) {
                std::vector<std::string> elemets;
                parse_line_v2(buffer, elemets);
                if (elemets.empty()) break;

                /*
                std::cout << "elemets.size() = " << elemets.size() << std::endl;
                for (size_t i = 0; i < elemets.size(); ++i) {
                    std::cout << "[" << i << "] = " << elemets[i] << std::endl;
                }
                */

                int flag = 0;
                if (elemets.size() == 7) {
                    flag = std::stoi(elemets[6]);
                } else
                if (elemets.size() == 5) {
                    if (elemets[2].empty()) flag = 4;
                    else if (elemets[3].empty()) flag = 2;
                    else flag = 6;
                }

                tick.timestamp_ms = change_timezone(ztime::to_timestamp_ms(elemets[0] + " " + elemets[1]));

                switch (flag) {
                case 2:
                    // читаем bid
                    tick.bid = std::stod(elemets[2]);
                    break;
                case 4:
                    // читаем ask
                    tick.ask = std::stod(elemets[3]);
                    break;
                case 6:
                    // читаем bid и ask
                    tick.bid = std::stod(elemets[2]);
                    tick.ask = std::stod(elemets[3]);
                    break;
                };
                if (on_tick && tick.bid && tick.ask) on_tick(tick);
            }
            return true;
        }

        inline bool parse_mt5_candles(std::ifstream &file) noexcept {
            // получаем заголовок файла
            std::string buffer;
            std::getline(file, buffer);
            Candle candle;
            while(!file.eof()) {
                std::getline(file, buffer);
                std::vector<std::string> elemets;
                parse_line(buffer, elemets);
                if (elemets.empty()) break;
                //const int flag = std::stoi(elemets.back());
                candle.timestamp = change_timezone(ztime::to_timestamp(elemets[0] + " " + elemets[1]));

                candle.open = std::stod(elemets[2]);
                candle.high = std::stod(elemets[3]);
                candle.low = std::stod(elemets[4]);
                candle.close = std::stod(elemets[5]);
                candle.volume = std::stod(elemets[6]);

                if (on_candle && !candle.empty()) on_candle(candle);
            }
            return true;
        }

    public:

        bool read() {
            std::ifstream file(config.file_name);
            if(!file.is_open()) {
                return false;
            }
            switch (config.type) {
            case QdbCsvType::MT5_CSV_TICKS_FILE:
                    /*
                    // получаем заголовок файла
                    std::getline(file, buffer);
                    Tick tick;
                    while(!file.eof()) {
                        std::getline(file, buffer);
                        std::vector<std::string> elemets;
                        parse_line(buffer, elemets);
                        if (elemets.empty()) break;
                        const int flag = std::stoi(elemets.back());
                        tick.timestamp_ms = change_timezone(ztime::to_timestamp_ms(elemets[0] + " " + elemets[1]));

                        switch (flag) {
                        case 2:
                            // читаем bid
                            tick.bid = std::stod(elemets[2]);
                            break;
                        case 4:
                            // читаем ask
                            tick.ask = std::stod(elemets[2]);
                            break;
                        case 6:
                            // читаем bid и ask
                            tick.bid = std::stod(elemets[2]);
                            tick.ask = std::stod(elemets[3]);
                            break;
                        };
                        if (on_tick && tick.bid && tick.ask) on_tick(tick);
                    }
                */
                return parse_mt5_ticks(file);
            case QdbCsvType::MT5_CSV_CANDLES_FILE:
                return parse_mt5_candles(file);
            }
            return false;
        };

    };
};

#endif // TRADING_DB_QDB_CSV_HPP_INCLUDED
