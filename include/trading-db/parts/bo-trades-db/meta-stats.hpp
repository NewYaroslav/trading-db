#pragma once
#ifndef TRADING_DB_BO_TRADES_DB_PARTS_META_BET_STATS_HPP_INCLUDED
#define TRADING_DB_BO_TRADES_DB_PARTS_META_BET_STATS_HPP_INCLUDED

#include "common.hpp"
#include "stats.hpp"

namespace trading_db {
    namespace bo_trades_db {
        /** \brief Класс для получения сведений о данных массива сделок
         */
        class MetaStats {
        public:
            std::vector<std::string>    brokers;
            std::vector<std::string>    symbols;
            std::vector<std::string>    signals;
            std::vector<std::string>    currencies;
            bool                        real        = false;
            bool                        demo        = false;

            std::vector<Stats>          currency_stats;
            std::vector<Stats>          signal_stats;
            std::vector<Stats>          broker_stats;
            std::vector<Stats>          symbol_stats;
            std::vector<Stats>          hour_stats;
            std::vector<Stats>          weekday_stats;

            MetaStats() {};

            std::function<double(const double value, const std::string &from)> on_convert = nullptr;

            double convert(const double value, const std::string &from) {
                if (on_convert) return on_convert(value, from);
                return value;
            }

            template<class T>
            void calc(const T &bets) noexcept {
                std::set<std::string> calc_currencies;
                std::set<std::string> calc_brokers;
                std::set<std::string> calc_signals;
                std::set<std::string> calc_symbols;

                for (auto &bet : bets) {
                    calc_currencies.insert(bet.currency);
                    calc_brokers.insert(bet.broker);
                    calc_signals.insert(bet.signal);
                    calc_symbols.insert(bet.symbol);
                    if (bet.demo) demo = true;
                    else real = true;
                }

                brokers = std::vector<std::string>(calc_brokers.begin(), calc_brokers.end());
                currencies = std::vector<std::string>(calc_currencies.begin(), calc_currencies.end());
                signals = std::vector<std::string>(calc_signals.begin(), calc_signals.end());
                symbols = std::vector<std::string>(calc_symbols.begin(), calc_symbols.end());

                currency_stats.clear();
                signal_stats.clear();
                broker_stats.clear();
                symbol_stats.clear();
                hour_stats.clear();
                weekday_stats.clear();

                currency_stats.resize(currencies.size());
                signal_stats.resize(signals.size());
                broker_stats.resize(brokers.size());
                symbol_stats.resize(symbols.size());
                hour_stats.resize(ztime::HOURS_PER_DAY);
                weekday_stats.resize(ztime::DAYS_PER_WEEK);


                for (size_t c = 0; c < currencies.size(); ++c) {
                    currency_stats[c].config.currency = currencies[c];
                    currency_stats[c].calc(bets, 0);
                }

                for (size_t s = 0; s < signals.size(); ++s) {
                    signal_stats[s].on_convert = [&](const double value, const std::string &from)->double {
                        return convert(value, from);
                    };
                    signal_stats[s].config.signals.push_back(signals[s]);
                    signal_stats[s].calc(bets, 0);
                }

                for (size_t s = 0; s < symbols.size(); ++s) {
                    symbol_stats[s].on_convert = [&](const double value, const std::string &from)->double {
                        return convert(value, from);
                    };
                    symbol_stats[s].config.symbols.push_back(symbols[s]);
                    symbol_stats[s].calc(bets, 0);
                }

                for (size_t s = 0; s < hour_stats.size(); ++s) {
                    hour_stats[s].on_convert = [&](const double value, const std::string &from)->double {
                        return convert(value, from);
                    };
                    hour_stats[s].config.use_hour = true;
                    hour_stats[s].config.hour = s;
                    hour_stats[s].calc(bets, 0);
                }

                for (size_t s = 0; s < weekday_stats.size(); ++s) {
                    weekday_stats[s].on_convert = [&](const double value, const std::string &from)->double {
                        return convert(value, from);
                    };
                    weekday_stats[s].config.use_weekday = true;
                    weekday_stats[s].config.weekday = s;
                    weekday_stats[s].calc(bets, 0);
                }

                for (size_t b = 0; b < brokers.size(); ++b) {
                    broker_stats[b].on_convert = [&](const double value, const std::string &from)->double {
                        return convert(value, from);
                    };
                    broker_stats[b].config.brokers.push_back(brokers[b]);
                    broker_stats[b].calc(bets, 0);
                }
            }
        }; // MetaStats
    }; // bo_trades_db
};

#endif // TRADING_DB_BO_TRADES_DB_PARTS_META_BET_STATS_HPP_INCLUDED
