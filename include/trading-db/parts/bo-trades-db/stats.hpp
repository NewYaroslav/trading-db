#pragma once
#ifndef TRADING_DB_BO_TRADES_DB_PARTS_BET_STATS_HPP_INCLUDED
#define TRADING_DB_BO_TRADES_DB_PARTS_BET_STATS_HPP_INCLUDED

#include "common.hpp"
#include <ztime.hpp>
#include <vector>
#include <map>
#include <set>

namespace trading_db {
	namespace bo_trades_db {
		/** \brief Статистика ставок
		 */
		class Stats {
		public:

			class WinrateStats {
			public:
				uint64_t	wins		= 0;
				uint64_t	losses		= 0;
				uint64_t	standoffs	= 0;
				uint64_t	deals		= 0;
				double		winrate		= 0;

				WinrateStats() {};

				WinrateStats(
					const uint64_t w,
					const uint64_t l,
					const uint64_t s) : wins(w), losses(l), standoffs(s) {};

				inline void win() noexcept {
					++wins;
				}

				inline void loss() noexcept {
					++losses;
				}

				inline void standoff() noexcept {
					++standoffs;
				}

				inline void calc() noexcept {
					deals = wins + losses + standoffs;
					winrate = deals == 0 ? 0.0 : (double)wins / (double)deals;
				}

				inline void clear() noexcept {
					wins		= 0;
					losses		= 0;
					standoffs	= 0;
					deals		= 0;
					winrate		= 0;
				}
			};

			class ChartData {
			public:
				std::vector<double>			y_data;
				std::vector<double>			x_data;
				std::vector<std::string>	x_label;

				inline void clear() noexcept {
					y_data.clear();
					x_data.clear();
					x_label.clear();
				}
			};

			/** \brief Критерий Серии
			 * Класс рассчитывает R и Z-счет ряда сделок
			 * Если сделок больше 30 шт., результаты Z-счета можно использовать
			 * Если Z-счет больше 1.96, можно судить о том что после прибыльной сделки скорее всего будет прибыльная
			 */
			class SeriesCriterion {
			private:
				int max_up = 0;
				int max_dn = 0;

				int wins = 0;
				int losses = 0;

				int row = 0;
				int series = 0;
				uint64_t t = 0;

			public:

				inline void update(const int value) noexcept {
					if (value > 0) {
						if (row == 0) {
							row = 1;
						} else
						if (row > 0) {
							++row;
						} else {
							max_dn = -row;
							row = 1;
							++series;
						}
						++wins;
					} else {
						if (row == 0) {
							row = -1;
						} else
						if (row < 0) {
							--row;
						} else {
							max_up = row;
							row = -1;
							++series;
						}
						++losses;
					}
				}

				inline bool update(
						const int value,
						const uint64_t start_time,
						const uint64_t stop_time) noexcept {
					if (stop_time < start_time) return false;
					if (t == 0) {
						t = stop_time;
						update(value);
					} else {
						if (start_time < t) return false;
						t = stop_time;
						update(value);
					}
					return true;
				}

				inline int get_r() noexcept {
					return row == 0 ? 0 : series + 1;
				}

				inline int get_trades() noexcept {
					return wins + losses;
				}

				inline double get_winrate() noexcept {
					const int sum = get_trades();
					return sum == 0 ? 0.0d : (double)wins / (double)sum;
				}

				inline double get_z_score() noexcept {
					const double d = (double)get_trades();
					const double w = get_winrate();
					const double r = (double)get_r();
					return ((double)r - 2.0 * w * (1.0 - w) * d) / (2.0 * w * (1.0 - w) * std::sqrt(d));
					// https://smart-lab.ru/company/stocksharp/blog/156276.php
					//const double x = 2 * wins * losses;
					//return (d * (r - 0.5) - x) / std::sqrt((x * (x - d))/(d - 1));
				}

				inline int get_max_row_up() noexcept {
					return max_up;
				}

				inline int get_max_row_dn() noexcept {
					return max_dn;
				}

				inline int get_wins() noexcept {
					return wins;
				}

				inline int get_losses() noexcept {
					return losses;
				}

				inline void clear() noexcept {
					max_up = 0;
					max_dn = 0;

					wins = 0;
					losses = 0;

					row = 0;
					series = 0;
					t = 0;
				}
			};

		private:

			SeriesCriterion series_criterion;

			// статистика по парам
			std::map<std::string, WinrateStats> stats_symbol;
			// статистика по сигналам
			std::map<std::string, WinrateStats> stats_signal;

			// статистика по периодам
			std::map<uint64_t, WinrateStats> stats_year;
			std::map<uint32_t, WinrateStats> stats_month;
			std::map<uint32_t, WinrateStats> stats_day_month;
			std::map<uint32_t, WinrateStats> stats_hour_day;
			std::map<uint32_t, WinrateStats> stats_minute_day;
			std::map<uint32_t, WinrateStats> stats_week_day;
			std::map<uint32_t, WinrateStats> stats_expiration;

			// для расчета статистики по пингу
			std::map<uint32_t, int> stats_ping;

			// статистика по кол-ву сигналов
			std::map<uint32_t, WinrateStats> stats_counter_bet;
			WinrateStats stats_temp_counter_bet;
			uint64_t counter_bet_timestamp = 0;
			uint32_t counter_bet = 1;

			// для построения графика баланса
			std::map<uint64_t, double> temp_balance;

			template<class T1, class T2>
			void calc_winrate(T1 &winrate, T2 &wins, T2 &losses) {
				for (size_t i = 0; i < winrate.size(); ++i) {
					double sum = (double)(wins[i] + losses[i]);
					winrate[i] = sum == 0 ? 0 : (double)wins[i] / sum;
				}
			}

		public:

			Stats() {
				clear();
			}

			class Config {
			public:
				// для фильтрации данных
				std::vector<std::string>	brokers;
				std::vector<std::string>	signals;
				std::string					currency;
				bool						use_demo	= true;		/// Использовать сделки на DEMO
				bool						use_real	= true;		/// Использовать сделки на REAL

				int							stats_type	= StatsTypes::ALL_BET;	///	 Тип статистики (первая сделка, последняя, все сделки)

			} config;

			// общая статистика
			WinrateStats total_stats;
			WinrateStats total_buy_stats;
			WinrateStats total_sell_stats;

			ChartData trades_profit;
			ChartData day_profit;
			ChartData trades_balance;
			ChartData day_balance;
			ChartData hour_balance;

			ChartData symbol_winrate;						/// Винрейт по символам
			ChartData symbol_trades;						/// Сделки по символам

			ChartData signal_winrate;						/// Винрейт по сигналам
			ChartData signal_trades;						/// Сделки по сигналам

			double total_volume						= 0;	/// Общий объем сделок
			double total_profit						= 0;	/// Общий профит
			double total_gain						= 0;	/// Общий прирост

			double max_drawdown						= 0;	/// Максимальная относительная просадка
			double max_absolute_drawdown			= 0;	/// Максимальная абсолютная просадка
			uint64_t max_drawdown_date				= 0;	/// Дата максимальной просадки
			double aver_drawdown					= 0;	/// Средняя относительная просадка

			double aver_trade_size					= 0;
			double aver_absolute_trade_size			= 0;

			double aver_profit_per_trade			= 0;	/// Средний относительный профит на сделку
			double aver_absolute_profit_per_trade	= 0;	/// Средний абсолютный профит на сделку
			double max_absolute_profit_per_trade	= 0;	/// Максимальный абсолютный профит на сделку

			double gross_profit						= 0;
			double gross_loss						= 0;
			double profit_factor					= 0;

			class ZScoreResult {
			public:
				double		value	= 0;
				double		winrate = 0;
				uint64_t	wins	= 0;
				uint64_t	losses	= 0;
				uint64_t	total_trades			= 0;
				uint64_t	max_consecutive_wins	= 0;
				uint64_t	max_consecutive_losses	= 0;

				inline void clear() noexcept {
					value	= 0;
					winrate	= 0;
					wins	= 0;
					losses	= 0;
					total_trades			= 0;
					max_consecutive_wins	= 0;
					max_consecutive_losses	= 0;
				}

			} z_score;

			// статистика профита
			std::array<double, 24> profit_24h;				/// Зависимость профита от часа дня
			std::array<double, 7>  profit_7wd;				/// Зависимость профита от дня недели
			std::array<double, 31> profit_31d;				/// Зависимость профита от дня месяца
			std::array<double, 12> profit_12m;				/// Зависимость профита от месяца года

			std::array<uint32_t, 24> trades_24h;			/// Зависимость кол-ва сделок от часа дня
			std::array<uint32_t, 7>	 trades_7wd;			/// Зависимость кол-ва сделок от дня недели
			std::array<uint32_t, 31> trades_31d;			/// Зависимость кол-ва сделок от дня месяца
			std::array<uint32_t, 12> trades_12m;			/// Зависимость кол-ва сделок от месяца года

			std::array<uint32_t, 24> wins_24h;				/// Зависимость кол-ва прибыльных сделок от часа дня
			std::array<uint32_t, 7>	 wins_7wd;				/// Зависимость кол-ва прибыльных сделок от дня недели
			std::array<uint32_t, 31> wins_31d;				/// Зависимость кол-ва прибыльных сделок от дня месяца
			std::array<uint32_t, 12> wins_12m;				/// Зависимость кол-ва прибыльных сделок от месяца года

			std::array<uint32_t, 24> losses_24h;			/// Зависимость кол-ва убыточных сделок от часа дня
			std::array<uint32_t, 7>	 losses_7wd;			/// Зависимость кол-ва убыточных сделок от дня недели
			std::array<uint32_t, 31> losses_31d;			/// Зависимость кол-ва убыточных сделок от дня месяца
			std::array<uint32_t, 12> losses_12m;			/// Зависимость кол-ва убыточных сделок от месяца года

			std::array<uint32_t, 24> standoffs_24h;			/// Зависимость кол-ва сделок ничья от часа дня
			std::array<uint32_t, 7>	 standoffs_7wd;			/// Зависимость кол-ва сделок ничья от дня недели
			std::array<uint32_t, 31> standoffs_31d;			/// Зависимость кол-ва сделок ничья от дня месяца
			std::array<uint32_t, 12> standoffs_12m;			/// Зависимость кол-ва сделок ничья от месяца года

			std::array<double, 24> winrate_24h;				/// Зависимость винрейта от часа дня
			std::array<double, 7>  winrate_7wd;				/// Зависимость винрейта от дня недели
			std::array<double, 31> winrate_31d;				/// Зависимость винрейта от дня месяца
			std::array<double, 12> winrate_12m;				/// Зависимость винрейта от месяца года

			std::array<double, 60> profit_60s;				/// Зависимость профита от секунды минуты
			std::array<double, 60> winrate_60s;				/// Зависимость винрейта от секунды минуты
			std::array<uint32_t, 60> wins_60s;				/// Зависимость кол-ва прибыльных сделок от секунды минуты
			std::array<uint32_t, 60> losses_60s;			/// Зависимость кол-ва убыточных сделок от секунды минуты
			std::array<uint32_t, 60> standoffs_60s;			/// Зависимость кол-ва сделок ничья от секунды минуты
			std::array<uint32_t, 60> trades_60s;			/// Зависимость кол-ва сделок от секунды минуты

			std::vector<double> winrate_ping_1s;
			std::vector<double> winrate_ping_50ms;
			std::vector<double> winrate_ping_100ms;
			std::vector<uint32_t> trades_ping_1s;
			std::vector<uint32_t> trades_ping_50ms;
			std::vector<uint32_t> trades_ping_100ms;

			inline void win(
					const std::string &symbol,
					const ContractType contract_type,
					const ztime::timestamp_t timestamp) noexcept {
				total_stats.win();
				stats_symbol[symbol].win();

				if (contract_type == ContractType::BUY) {
					total_buy_stats.win();
				} else {
					total_sell_stats.win();
				}

				stats_year[ztime::start_of_year(timestamp)].win();
				stats_month[ztime::get_month(timestamp)].win();
				stats_day_month[ztime::get_day_month(timestamp)].win();
				stats_hour_day[ztime::get_hour_day(timestamp)].win();
				stats_minute_day[ztime::get_minute_day(timestamp)].win();
				stats_week_day[ztime::get_weekday(timestamp)].win();

				if (counter_bet_timestamp == 0) counter_bet_timestamp = ztime::get_first_timestamp_minute(timestamp);

				if (counter_bet_timestamp == ztime::get_first_timestamp_minute(timestamp)) {
					stats_temp_counter_bet.win();
					++counter_bet;
				} else {
					counter_bet_timestamp = ztime::get_first_timestamp_minute(timestamp);
					stats_counter_bet[counter_bet] = stats_temp_counter_bet;
					stats_temp_counter_bet.clear();
					stats_temp_counter_bet.win();
					counter_bet = 1;
				}
			}

			inline void loss(
					const std::string &symbol,
					const ContractType contract_type,
					const ztime::timestamp_t timestamp) noexcept {

				total_stats.loss();
				stats_symbol[symbol].loss();

				if (contract_type == ContractType::BUY) {
					total_buy_stats.loss();
				} else {
					total_sell_stats.loss();
				}

				stats_year[ztime::start_of_year(timestamp)].loss();
				stats_month[ztime::get_month(timestamp)].loss();
				stats_day_month[ztime::get_day_month(timestamp)].loss();
				stats_hour_day[ztime::get_hour_day(timestamp)].loss();
				stats_minute_day[ztime::get_minute_day(timestamp)].loss();
				stats_week_day[ztime::get_weekday(timestamp)].loss();

				if (counter_bet_timestamp == 0) counter_bet_timestamp = ztime::get_first_timestamp_minute(timestamp);

				if (counter_bet_timestamp == ztime::get_first_timestamp_minute(timestamp)) {
					stats_temp_counter_bet.loss();
					++counter_bet;
				} else {
					counter_bet_timestamp = ztime::get_first_timestamp_minute(timestamp);
					stats_counter_bet[counter_bet] = stats_temp_counter_bet;
					stats_temp_counter_bet.clear();
					stats_temp_counter_bet.loss();
					counter_bet = 1;
				}
			}

			inline void standoff(
					const std::string &symbol,
					const ContractType contract_type,
					const ztime::timestamp_t timestamp) noexcept {

				total_stats.standoff();
				stats_symbol[symbol].standoff();

				if (contract_type == ContractType::BUY) {
					total_buy_stats.standoff();
				} else {
					total_sell_stats.standoff();
				}

				stats_year[ztime::start_of_year(timestamp)].standoff();
				stats_month[ztime::get_month(timestamp)].standoff();
				stats_day_month[ztime::get_day_month(timestamp)].standoff();
				stats_hour_day[ztime::get_hour_day(timestamp)].standoff();
				stats_minute_day[ztime::get_minute_day(timestamp)].standoff();
				stats_week_day[ztime::get_weekday(timestamp)].standoff();

				if (counter_bet_timestamp == 0) counter_bet_timestamp = ztime::get_first_timestamp_minute(timestamp);

				if (counter_bet_timestamp == ztime::get_first_timestamp_minute(timestamp)) {
					stats_temp_counter_bet.standoff();
					++counter_bet;
				} else {
					counter_bet_timestamp = ztime::get_first_timestamp_minute(timestamp);
					stats_counter_bet[counter_bet] = stats_temp_counter_bet;
					stats_temp_counter_bet.clear();
					stats_temp_counter_bet.standoff();
					counter_bet = 1;
				}
			}

			void clear() noexcept {
				temp_balance.clear();

				stats_symbol.clear();

				stats_signal.clear();

				stats_temp_counter_bet.clear();
				counter_bet_timestamp	= 0;
				counter_bet				= 1;

				// статистика по периодам
				stats_year.clear();
				stats_month.clear();
				stats_day_month.clear();
				stats_hour_day.clear();
				stats_minute_day.clear();
				stats_week_day.clear();
				stats_expiration.clear();
				// одновременные сделки
				stats_counter_bet.clear();
				// статистика по всему
				total_stats.clear();
				total_buy_stats.clear();
				total_sell_stats.clear();
				// график
				trades_profit.clear();
				day_profit.clear();
				trades_balance.clear();
				day_balance.clear();
				hour_balance.clear();
				//
				symbol_winrate.clear();
				symbol_trades.clear();
				//
				signal_winrate.clear();
				signal_trades.clear();
				// остальное
				total_volume					= 0;
				total_profit					= 0;
				total_gain						= 0;

				max_drawdown					= 0;
				max_absolute_drawdown			= 0;
				max_drawdown_date				= 0;
				aver_drawdown					= 0;

				aver_profit_per_trade			= 0;
				aver_absolute_profit_per_trade	= 0;
				max_absolute_profit_per_trade	= 0;

				gross_profit					= 0;
				gross_loss						= 0;
				profit_factor					= 0;

				stats_ping.clear();

				winrate_ping_1s.clear();
				winrate_ping_50ms.clear();
				winrate_ping_100ms.clear();

				trades_ping_1s.clear();
				trades_ping_50ms.clear();
				trades_ping_100ms.clear();

				//
				z_score.clear();
				series_criterion.clear();

				//
				std::fill(profit_24h.begin(), profit_24h.end(), 0.0d);
				std::fill(profit_7wd.begin(), profit_7wd.end(), 0.0d);
				std::fill(profit_31d.begin(), profit_31d.end(), 0.0d);
				std::fill(profit_12m.begin(), profit_12m.end(), 0.0d);
				std::fill(profit_60s.begin(), profit_60s.end(), 0.0d);

				std::fill(trades_24h.begin(), trades_24h.end(), 0);
				std::fill(trades_7wd.begin(), trades_7wd.end(), 0);
				std::fill(trades_31d.begin(), trades_31d.end(), 0);
				std::fill(trades_12m.begin(), trades_12m.end(), 0);
				std::fill(trades_60s.begin(), trades_60s.end(), 0);

				std::fill(wins_24h.begin(), wins_24h.end(), 0);
				std::fill(wins_7wd.begin(), wins_7wd.end(), 0);
				std::fill(wins_31d.begin(), wins_31d.end(), 0);
				std::fill(wins_12m.begin(), wins_12m.end(), 0);
				std::fill(wins_60s.begin(), wins_60s.end(), 0);

				std::fill(losses_24h.begin(), losses_24h.end(), 0);
				std::fill(losses_7wd.begin(), losses_7wd.end(), 0);
				std::fill(losses_31d.begin(), losses_31d.end(), 0);
				std::fill(losses_12m.begin(), losses_12m.end(), 0);
				std::fill(losses_60s.begin(), losses_60s.end(), 0);

				std::fill(standoffs_24h.begin(), standoffs_24h.end(), 0);
				std::fill(standoffs_7wd.begin(), standoffs_7wd.end(), 0);
				std::fill(standoffs_31d.begin(), standoffs_31d.end(), 0);
				std::fill(standoffs_12m.begin(), standoffs_12m.end(), 0);
				std::fill(standoffs_60s.begin(), standoffs_60s.end(), 0);

				std::fill(winrate_24h.begin(), winrate_24h.end(), 0.0d);
				std::fill(winrate_7wd.begin(), winrate_7wd.end(), 0.0d);
				std::fill(winrate_31d.begin(), winrate_31d.end(), 0.0d);
				std::fill(winrate_12m.begin(), winrate_12m.end(), 0.0d);
				std::fill(winrate_60s.begin(), winrate_60s.end(), 0.0d);

			}

			template<class T>
			void calc(const T &bets, const double start_balance) noexcept {
				clear();

				size_t counter_bet = 0;
				double profit = 0;

				for (auto &bet : bets) {

					if (config.stats_type == FIRST_BET && bet.step != 0) continue;
					if (config.stats_type == LAST_BET && !bet.last) continue;
					if (!config.currency.empty() && bet.currency != config.currency) continue;

					if (!config.brokers.empty()) {
						bool found = false;
						for (size_t i = 0; i < config.brokers.size(); ++i) {
							if (config.brokers[i] == bet.broker) {
								found = true;
								break;
							}
						}
						if (!found) continue;
					}

					if (!config.signals.empty()) {
						bool found = false;
						for (size_t i = 0; i < config.signals.size(); ++i) {
							if (config.signals[i] == bet.signal) {
								found = true;
								break;
							}
						}
						if (!found) continue;
					}

					if (bet.demo && !config.use_demo) continue;
					if (!bet.demo && !config.use_real) continue;
					if (bet.amount == 0) continue;

					const ztime::timestamp_t timestamp = ztime::ms_to_sec(bet.open_date);
					const ztime::timestamp_t end_timestamp = ztime::ms_to_sec(bet.close_date);
					const ztime::timestamp_t first_timestamp_day = ztime::start_of_day(timestamp);

					const size_t second = ztime::get_second_minute(timestamp);
					const size_t hour = ztime::get_hour_day(timestamp);
					const size_t weekday = ztime::get_weekday(timestamp);
					const size_t day_month = ztime::get_day_month(timestamp);
					const size_t month = ztime::get_month(timestamp);

					if (bet.status == BoStatus::WIN) {
						win(bet.symbol, bet.contract_type, timestamp);

						stats_signal[bet.signal].win();

						// считаем число серий
						series_criterion.update(1, timestamp, end_timestamp);

						// считаем статистику для пинга
						stats_ping[bet.ping] = 1;

						profit += bet.profit;

						trades_profit.y_data.push_back(profit);
						trades_profit.x_data.push_back(timestamp);

						if (day_profit.x_data.empty() || day_profit.x_data.back() != first_timestamp_day) {
							day_profit.x_data.push_back(first_timestamp_day);
							day_profit.y_data.push_back(bet.profit);
						} else {
							day_profit.y_data.back() += bet.profit;
						}

						if (temp_balance.find(timestamp) == temp_balance.end()) {
							temp_balance[timestamp] = -bet.amount;
						} else {
							temp_balance[timestamp] += -bet.amount;
						}
						if (temp_balance.find(end_timestamp) == temp_balance.end()) {
							temp_balance[end_timestamp] = (bet.amount + bet.profit);
						} else {
							temp_balance[end_timestamp] += (bet.amount + bet.profit);
						}

						aver_profit_per_trade += bet.profit / bet.amount;
						aver_absolute_profit_per_trade += bet.profit;
						++counter_bet;
						if (bet.profit > max_absolute_profit_per_trade) max_absolute_profit_per_trade = bet.profit;

						gross_profit += bet.profit;
						total_profit += bet.profit;
						total_volume += bet.amount;

						profit_60s[second] += bet.profit;
						profit_24h[hour] += bet.profit;
						profit_7wd[weekday] += bet.profit;
						profit_31d[day_month - 1] += bet.profit;
						profit_12m[month - 1] += bet.profit;

						trades_60s[second] += 1;
						trades_24h[hour] += 1;
						trades_7wd[weekday] += 1;
						trades_31d[day_month - 1] += 1;
						trades_12m[month - 1] += 1;

						wins_60s[second] += 1;
						wins_24h[hour] += 1;
						wins_7wd[weekday] += 1;
						wins_31d[day_month - 1] += 1;
						wins_12m[month - 1] += 1;

						aver_absolute_trade_size += bet.amount;
					} else
					if (bet.status == BoStatus::LOSS) {
						loss(bet.symbol, bet.contract_type, timestamp);

						stats_signal[bet.signal].loss();

						// считаем число серий
						series_criterion.update(-1, timestamp, end_timestamp);

						// считаем статистику для пинга
						stats_ping[bet.ping] = -1;

						profit -= bet.amount;

						trades_profit.y_data.push_back(profit);
						trades_profit.x_data.push_back(timestamp);

						if (day_profit.x_data.empty() || day_profit.x_data.back() != first_timestamp_day) {
							day_profit.x_data.push_back(first_timestamp_day);
							day_profit.y_data.push_back(-bet.amount);
						} else {
							day_profit.y_data.back() += -bet.amount;
						}

						if (temp_balance.find(timestamp) == temp_balance.end()) {
							temp_balance[timestamp] = -bet.amount;
						} else {
							temp_balance[timestamp] += -bet.amount;
						}
						if (temp_balance.find(end_timestamp) == temp_balance.end()) {
							temp_balance[end_timestamp] = 0;
						} else {
							temp_balance[end_timestamp] += 0;
						}

						aver_profit_per_trade -= 1.0;
						aver_absolute_profit_per_trade -= bet.amount;
						++counter_bet;

						gross_loss += bet.amount;
						total_profit -= bet.amount;
						total_volume += bet.amount;

						profit_60s[second] -= bet.amount;
						profit_24h[hour] -= bet.amount;
						profit_7wd[weekday] -= bet.amount;
						profit_31d[day_month - 1] -= bet.amount;
						profit_12m[month - 1] -= bet.amount;

						trades_60s[second] += 1;
						trades_24h[hour] += 1;
						trades_7wd[weekday] += 1;
						trades_31d[day_month - 1] += 1;
						trades_12m[month - 1] += 1;

						losses_60s[second] += 1;
						losses_24h[hour] += 1;
						losses_7wd[weekday] += 1;
						losses_31d[day_month - 1] += 1;
						losses_12m[month - 1] += 1;

						aver_absolute_trade_size += bet.amount;
					} else
					if (bet.status == BoStatus::STANDOFF) {
						standoff(bet.symbol, bet.contract_type, timestamp);

						stats_signal[bet.signal].standoff();

						// считаем число серий
						series_criterion.update(0, timestamp, end_timestamp);

						// считаем статистику для пинга
						stats_ping[bet.ping] = 0;

						trades_profit.y_data.push_back(profit);
						trades_profit.x_data.push_back(timestamp);

						if (day_profit.x_data.empty() || day_profit.x_data.back() != first_timestamp_day) {
							day_profit.x_data.push_back(first_timestamp_day);
							day_profit.y_data.push_back(0);
						}

						if (temp_balance.find(timestamp) == temp_balance.end()) {
							temp_balance[timestamp] = -bet.amount;
						} else {
							temp_balance[timestamp] += -bet.amount;
						}
						if (temp_balance.find(end_timestamp) == temp_balance.end()) {
							temp_balance[end_timestamp] = bet.amount;
						} else {
							temp_balance[end_timestamp] += bet.amount;
						}

						++counter_bet;

						total_volume += bet.amount;

						trades_60s[second] += 1;
						trades_24h[hour] += 1;
						trades_7wd[weekday] += 1;
						trades_31d[day_month - 1] += 1;
						trades_12m[month - 1] += 1;

						standoffs_60s[second] += 1;
						standoffs_24h[hour] += 1;
						standoffs_7wd[weekday] += 1;
						standoffs_31d[day_month - 1] += 1;
						standoffs_12m[month - 1] += 1;

						aver_absolute_trade_size += bet.amount;
					}
				}

				// вычисляем винрейт
				calc_winrate(winrate_60s, wins_60s, losses_60s);
				calc_winrate(winrate_24h, wins_24h, losses_24h);
				calc_winrate(winrate_7wd, wins_7wd, losses_7wd);
				calc_winrate(winrate_31d, wins_31d, losses_31d);
				calc_winrate(winrate_12m, wins_12m, losses_12m);

				// вычисляем Z-счет
				z_score.value	= series_criterion.get_z_score();
				z_score.winrate	= series_criterion.get_winrate();
				z_score.wins	= series_criterion.get_wins();
				z_score.losses	= series_criterion.get_losses();
				z_score.total_trades			= series_criterion.get_trades();
				z_score.max_consecutive_wins	= series_criterion.get_max_row_up();
				z_score.max_consecutive_losses	= series_criterion.get_max_row_dn();

				// вычисляем зависимость винрейта от пинга
				if (!stats_ping.empty()) {
					std::vector<WinrateStats> trades_1s;
					std::vector<WinrateStats> trades_50ms;
					std::vector<WinrateStats> trades_100ms;

					for (auto &item : stats_ping) {
						const int index_1s = item.first < 0 ? 0 : (item.first / 1000);
						const int index_50ms = item.first < 0 ? 0 : (item.first / 50);
						const int index_100ms = item.first < 0 ? 0 : (item.first / 100);

						if (index_1s >= trades_1s.size()) trades_1s.resize(index_1s + 1);
						if (index_50ms >= trades_50ms.size()) trades_50ms.resize(index_50ms + 1);
						if (index_100ms >= trades_100ms.size()) trades_100ms.resize(index_100ms + 1);

						const int result = item.second;

						if (result > 0) {
							trades_1s[index_1s].win();
							trades_50ms[index_50ms].win();
							trades_100ms[index_100ms].win();
						} else {
							trades_1s[index_1s].loss();
							trades_50ms[index_50ms].loss();
							trades_100ms[index_100ms].loss();
						}
					}

					winrate_ping_1s.resize(trades_1s.size());
					winrate_ping_50ms.resize(trades_50ms.size());
					winrate_ping_100ms.resize(trades_100ms.size());

					trades_ping_1s.resize(trades_1s.size());
					trades_ping_50ms.resize(trades_50ms.size());
					trades_ping_100ms.resize(trades_100ms.size());

					for (size_t i = 0; i < trades_1s.size(); ++i) {
						trades_1s[i].calc();
						winrate_ping_1s[i] = trades_1s[i].winrate;
						trades_ping_1s[i] = trades_1s[i].deals;
					}

					for (size_t i = 0; i < trades_50ms.size(); ++i) {
						trades_50ms[i].calc();
						winrate_ping_50ms[i] = trades_50ms[i].winrate;
						trades_ping_50ms[i] = trades_50ms[i].deals;
					}

					for (size_t i = 0; i < trades_100ms.size(); ++i) {
						trades_100ms[i].calc();
						winrate_ping_100ms[i] = trades_100ms[i].winrate;
						trades_ping_100ms[i] = trades_100ms[i].deals;
					}
				}

				if (counter_bet) {
					aver_absolute_profit_per_trade /= (double)counter_bet;
					aver_profit_per_trade /= (double)counter_bet;
					aver_absolute_trade_size /= (double)counter_bet;
				}

				if (gross_loss > 0) {
					profit_factor = gross_profit / gross_loss;
				} else {
					profit_factor = gross_profit > 0 ? std::numeric_limits<double>::max() : 0;
				}

				// рисуем график баланса
				if (!temp_balance.empty() && start_balance > 0) {

					double balance = start_balance;
					double last_max_balance = start_balance;
					double diff_balance = 0;

					max_absolute_drawdown = 0;
					max_drawdown = 0;
					aver_drawdown = 0;
					size_t counter_aver_drawdown = 0;

					bool is_drawdown = false;

					trades_balance.x_data.push_back(ztime::start_of_day(temp_balance.begin()->first));
					trades_balance.y_data.push_back(balance);
					for (auto &b : temp_balance) {
						const double prev_balance = balance;
						balance += b.second;
						trades_balance.x_data.push_back(b.first);
						trades_balance.y_data.push_back(balance);

						if (balance < last_max_balance) {
							// замер разницы депозита между мин. и макс.
							diff_balance = last_max_balance - balance;
							// замер абсолютной и относительной просадки
							if (diff_balance > max_absolute_drawdown) {
								max_absolute_drawdown = diff_balance;
								max_drawdown = max_absolute_drawdown / last_max_balance;
								// запоминаемдату начала просадки
								if (!is_drawdown) max_drawdown_date = b.first;
							}
							// ставим флаг просадки
							is_drawdown = true;
						}
						if (balance >= last_max_balance) {
							// замер средней просадки
							if (is_drawdown) {
								aver_drawdown += (diff_balance / last_max_balance);
								++counter_aver_drawdown;
							}
							// сброс флагов и параметров
							last_max_balance = balance;
							is_drawdown = false;
						}
					}
					if (counter_aver_drawdown) aver_drawdown /= (double)counter_aver_drawdown;
					total_gain = balance / start_balance;
				} // if (!temp_balance.empty() && start_balance != 0)

				total_stats.calc();
				total_buy_stats.calc();
				total_sell_stats.calc();

				//> винрейт по символам
				for (auto &item : stats_symbol) {
					item.second.calc();
					symbol_winrate.x_label.push_back(item.first);
					symbol_winrate.y_data.push_back(item.second.winrate * 100);
					symbol_trades.x_label.push_back(item.first);
					symbol_trades.y_data.push_back(item.second.deals);
				} //<

				//> винрейт по сигналам
				for (auto &item : stats_signal) {
					item.second.calc();
					signal_winrate.x_label.push_back(item.first);
					signal_winrate.y_data.push_back(item.second.winrate * 100);
					signal_trades.x_label.push_back(item.first);
					signal_trades.y_data.push_back(item.second.deals);
				} //<

			}
		}; // Stats
	}; // bo_trades_db
};

#endif // TRADING_DB_BO_TRADES_DB_PARTS_BET_STATS_HPP_INCLUDED
