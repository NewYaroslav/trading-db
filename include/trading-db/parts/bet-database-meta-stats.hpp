#pragma once
#ifndef TRADING_DB_BET_DATABASE_PARTS_META_BET_STATS_HPP_INCLUDED
#define TRADING_DB_BET_DATABASE_PARTS_META_BET_STATS_HPP_INCLUDED

#include "bet-database-common.hpp"
#include "bet-database-stats.hpp"

namespace trading_db {
	namespace bet_database {
		/** \brief Класс для получения сведений о данных массива сделок
		 */
		class BetMetaStats {
		public:
			std::vector<std::string>	brokers;
			std::vector<std::string>	symbols;
			std::vector<std::string>	signals;
			std::vector<std::string>	currencies;
			bool						real		= false;
			bool						demo		= false;

			std::vector<BetStats>		currency_stats;
			std::vector<BetStats>		signals_stats;
			std::vector<BetStats>		brokers_stats;

			BetMetaStats() {};

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
				signals_stats.clear();
				brokers_stats.clear();

				currency_stats.resize(currencies.size());
				signals_stats.resize(signals.size());
				brokers_stats.resize(brokers.size());

				for (size_t c = 0; c < currencies.size(); ++c) {
					currency_stats[c].config.currency = currencies[c];
					currency_stats[c].calc(bets, 0);
				}

				for (size_t s = 0; s < signals.size(); ++s) {
					signals_stats[s].config.signals.push_back(signals[s]);
					signals_stats[s].calc(bets, 0);
				}
				for (size_t b = 0; b < brokers.size(); ++b) {
					brokers_stats[b].config.brokers.push_back(brokers[b]);
					brokers_stats[b].calc(bets, 0);
				}
			}
		}; // BetMetaStats
	};
};

#endif // TRADING_DB_BET_DATABASE_PARTS_META_BET_STATS_HPP_INCLUDED
