#pragma once
#ifndef TRADING_DB_QDB_FX_SYMBOLS_DB_HPP_INCLUDED
#define TRADING_DB_QDB_FX_SYMBOLS_DB_HPP_INCLUDED

#include "../../parts/qdb/enums.hpp"
#include "../../parts/qdb/data-classes.hpp"
#include "../../qdb.hpp"
#include "../../utils/async-tasks.hpp"
#include "ztime.hpp"
#include <vector>
#include <set>

namespace trading_db {

	/** \brief База данных символов для работы с форексом
	 */
	class QdbFxSymbolDB {
	public:

		class SymbolConfig {
		public:
			std::string symbol;
			std::string base;
			std::string quote;
			size_t prefix_count = 0;
			double point = 0.0;
			double contract_size = 0.0;

			SymbolConfig() {};

			SymbolConfig(const std::string &s, const size_t pc = 0, const double p = 0.0, const double cz = 0.0) :
				symbol(s), prefix_count(pc), point(p), contract_size(cz) {
			}
		};

		/** \brief Конфигурация тестера
		 */
		class Config {
		public:
			std::string					path_db;						/**< Путь к папке с БД котировок */
			std::vector<SymbolConfig>	symbols;						/**< Массив символов */
			std::string					account_currency = "USD";		/**< Валюта депозита */
			double 						account_leverage = 100;			/**< Кредитное плечо */

			std::function<void(const std::string &msg)>	on_msg	= nullptr;
		}; // Config

		/** \brief Параметры сделки FX
		 */
		class TradeFxSignal {
		public:
			std::string symbol;                 /**< (Или) Имя символа */
			size_t		symbol_index    = 0;	/**< (Или) Индекс символа */
			uint64_t	open_date_ms    = 0;	/**< Время открытия сделки */
			uint64_t	close_date_ms	= 0;	/**< Время закрытия сделки */
			uint32_t	open_delay_ms   = 0;	/**< Задержка на вход в сделку */
			uint32_t	close_delay_ms	= 0;	/**< Задержка на выход из сделки */
			uint32_t    duration_ms	    = 0;	/**< Экспирация */
			double      lot_size        = 0;    /**< Размер лота */
			bool		direction	    = false;/**< Направление "на повышение"/"на понижение" */
		};

		/** \brief Результат сделки FX
		 */
		class TradeFxResult {
		public:
			double	open_price	    = 0.0;		/**< Цена открытия */
			double	close_price     = 0.0;		/**< Цена закрытия */
			double	send_date_ms	= 0;		/**< Дата запроса */
			double	open_date_ms	= 0;		/**< Дата открытия сделки */
			double	close_date_ms   = 0;		/**< Дата закрытия сделки */
			double	profit		    = 0.0;		/**< Профит */
			double	pips		    = 0.0;		/**< Количество пипсов */
			bool	win			    = false;    /**< Результат сделки "победа"/"поражение" */
			bool	success		    = false;    /**< Флаг инициализации результата сделки (для проверки достоверности данных) */
		};

	private:
		Config									m_config;
		std::vector<std::shared_ptr<QDB>>		m_symbol_db;
		std::map<std::string, size_t>			m_currency_to_index; // соотношение (валюта)-(индекс валюты)
		std::vector<std::pair<size_t,size_t>>	m_symbol_currency;
		std::vector<size_t>						m_cross_symbol;
		std::vector<size_t>						m_cross_symbol_invert;
		size_t 									m_account_currency_index = 0;
		std::map<std::string, size_t>			m_symbol_to_index;

		// Инициализация базы данных
		inline bool init_db() {
			m_symbol_db.clear();
			for (size_t s = 0; s < m_config.symbols.size(); ++s) {
				m_symbol_db.push_back(std::make_shared<trading_db::QDB>());
				const std::string file_name = m_config.path_db + "\\" + m_config.symbols[s].symbol + ".qdb";
				if (!m_symbol_db[s]->open(file_name, true)) {
					if (m_config.on_msg) m_config.on_msg("Database opening error! File name: " + file_name);
					return false;
				}
			}
			return true;
		}

		// Инициализация конфигураций символов
		inline bool init_config() {
			for (size_t s = 0; s < m_config.symbols.size(); ++s) {
				m_config.symbols[s].base = m_config.symbols[s].symbol.substr(m_config.symbols[s].prefix_count,3);
				m_config.symbols[s].quote = m_config.symbols[s].symbol.substr(m_config.symbols[s].prefix_count+3,3);
				if (m_config.symbols[s].point == 0.0) {
					if (m_config.symbols[s].base == "JPY" ||
						m_config.symbols[s].quote == "JPY") {
						m_config.symbols[s].point = 0.001;
					} else {
						m_config.symbols[s].point = 0.00001;
					}
				}
				if (m_config.symbols[s].contract_size == 0.0) {
					m_config.symbols[s].contract_size = 100000.0;
				}
			}
			return true;
		}

		// Инициализация индексов
		inline bool init_indexs() {
			// Присваиваем индексы валютам
			m_symbol_currency.resize(m_config.symbols.size());
			size_t currency_index = 0;
			for (size_t s = 0; s < m_config.symbols.size(); ++s) {
				const std::string &base = m_config.symbols[s].base;
				const std::string &quote = m_config.symbols[s].quote;
				auto it = m_currency_to_index.find(base);
				if (it == m_currency_to_index.end()) {
					m_currency_to_index[base] = currency_index++;
				}
				it = m_currency_to_index.find(quote);
				if (it == m_currency_to_index.end()) {
					m_currency_to_index[quote] = currency_index++;
				}
				const size_t base_index = m_currency_to_index[base];
				const size_t quote_index = m_currency_to_index[quote];
				m_symbol_currency[s] = std::make_pair(base_index, quote_index);
				m_symbol_to_index[m_config.symbols[s].symbol] = s;
			}

			// Проверяем наличие валюты аккаунта среди доступных валютных пар
			auto it = m_currency_to_index.find(m_config.account_currency);
			if (it == m_currency_to_index.end()) return false;
			m_account_currency_index = it->second;

			// Ищем символы для конвертации для кросс-валютных пар
			m_cross_symbol.resize(m_config.symbols.size(), 0);
			m_cross_symbol_invert.resize(m_config.symbols.size(), 0);

			for (size_t j = 0; j < m_config.symbols.size(); ++j) {

				m_cross_symbol[j] = m_config.symbols.size();
				m_cross_symbol_invert[j] = m_config.symbols.size();

				for (size_t i = 0; i < m_config.symbols.size(); ++i) {
					// Валюта депозита находится в нижней части переводного курса
					if (m_symbol_currency[i].second == m_account_currency_index &&
						m_symbol_currency[i].first == m_symbol_currency[j].second) {
						m_cross_symbol[j] = i;
						break;
					}
					// Валюта депозита находится в верхней части переводного курса
					if (m_symbol_currency[i].first == m_account_currency_index &&
						m_symbol_currency[i].second == m_symbol_currency[j].second) {
						m_cross_symbol_invert[j] = i;
						break;
					}
				}
			}
			return true;
		}

		// Рассчет профита, реализовано по статье: https://www.mql5.com/ru/articles/10211
		inline bool calc_profit(
				const size_t s_index,
				const double lot,
				const uint64_t t_open_ms,
				const uint64_t t_close_ms,
				const bool direction,
				double &open_price,
				double &close_price,
				double &profit) {
			// Получаем актуальные цены
			trading_db::Tick open_tick, close_tick;
			if (!m_symbol_db[s_index]->get_tick_ms(open_tick, t_open_ms)) return false;
			if (!m_symbol_db[s_index]->get_tick_ms(close_tick, t_close_ms)) return false;

			open_price = direction ? open_tick.ask : open_tick.bid;
			close_price = direction ? close_tick.bid : close_tick.ask;

			double mult = lot * m_config.symbols[s_index].contract_size * m_config.account_leverage;
			if (direction) {
                mult *= (close_price - open_price);
            } else {
                mult *= (open_price - close_price);
            }

			// Валюта инструмента совпадает с валютой нашего депозита
			if (m_symbol_currency[s_index].second == m_account_currency_index) {
				profit = mult;
				return true;
			}
			// Все остальные случаи (ищем переводной курс)
			// Важно: вычисления при закрытии позиции гораздо более точные

			// Валюта депозита находится в нижней части переводного курса
			if (m_cross_symbol[s_index] < m_config.symbols.size()) {
				trading_db::Tick last_tick;
				if (!m_symbol_db[m_cross_symbol[s_index]]->get_tick_ms(last_tick, t_close_ms)) return false;
				profit = mult * last_tick.bid;
				return true;
			}
			// Валюта депозита находится в верхней части переводного курса
			if (m_cross_symbol_invert[s_index] < m_config.symbols.size()) {
				trading_db::Tick last_tick;
				if (!m_symbol_db[m_cross_symbol_invert[s_index]]->get_tick_ms(last_tick, t_close_ms)) return false;
				profit = mult / last_tick.ask;
				return true;
			}
			return false;
		}

	public:

		QdbFxSymbolDB() {};
		~QdbFxSymbolDB() {};

		inline Config get_config() noexcept {
			return m_config;
		}

		inline void set_config(const Config &arg_config) noexcept {
			m_config = arg_config;
		}

		inline bool init() noexcept {
			if (!init_db()) return false;
			if (!init_config()) return false;
			if (!init_indexs()) return false;
			return true;
		}

		bool calc_trade_result(
				const TradeFxSignal &signal,	// Сигнал
				TradeFxResult		&result		// Результат сигнала
				) {

			if (signal.close_date_ms != 0 && signal.close_date_ms < signal.open_date_ms) return false;

			const uint64_t open_date_ms		= signal.open_date_ms + signal.open_delay_ms;
			const uint64_t close_date_ms	= signal.close_date_ms == 0 ?
                (open_date_ms + signal.duration_ms + signal.close_delay_ms) :
                (signal.close_date_ms + signal.close_delay_ms);

			result.send_date_ms = signal.open_date_ms;
			result.open_date_ms = open_date_ms;
			result.close_date_ms = close_date_ms;
			result.success = false;
			result.win = false;

			size_t s_index = 0;
			if (signal.symbol.empty()) {
				s_index = signal.symbol_index;
			} else {
				auto it = m_symbol_to_index.find(signal.symbol);
				if (it == m_symbol_to_index.end()) return false;
				s_index = it->second;
			}

			if (!calc_profit(
				s_index,
				signal.lot_size,
				open_date_ms,
				close_date_ms,
				signal.direction,
				result.open_price,
				result.close_price,
				result.profit)) {
				return false;
			}
			result.pips = signal.direction ? (result.close_price - result.open_price) : (result.open_price - result.close_price);
			result.pips /= m_config.symbols[s_index].point;
			result.win = (result.profit > 0);
			result.success = true;
			return true;
		}

		inline bool get_candle(
				Candle &candle,
                const size_t s_index,
				const uint64_t t,
                const QDB_TIMEFRAMES p = QDB_TIMEFRAMES::PERIOD_M1,
                const QDB_CANDLE_MODE m = QDB_CANDLE_MODE::SRC_CANDLE) noexcept {
            return m_symbol_db[s_index]->get_candle(candle, t, p, m);
        }

		inline bool get_candle(
				Candle &candle,
                const std::string &symbol,
				const uint64_t t,
                const QDB_TIMEFRAMES p = QDB_TIMEFRAMES::PERIOD_M1,
                const QDB_CANDLE_MODE m = QDB_CANDLE_MODE::SRC_CANDLE) noexcept {
            auto it = m_symbol_to_index.find(symbol);
			if (it == m_symbol_to_index.end()) return false;
			return m_symbol_db[it->second]->get_candle(candle, t, p, m);
        }

        inline bool get_tick(Tick &tick, const size_t s_index, const uint64_t t) noexcept {
            return m_symbol_db[s_index]->get_tick(tick, t);
        }

		inline bool get_tick(Tick &tick, const std::string &symbol, const uint64_t t) noexcept {
            auto it = m_symbol_to_index.find(symbol);
			if (it == m_symbol_to_index.end()) return false;
			return m_symbol_db[it->second]->get_tick(tick, t);
        }

        inline bool get_tick_ms(Tick &tick, const size_t s_index, const uint64_t t_ms) noexcept {
            return m_symbol_db[s_index]->get_tick_ms(tick, t_ms);
        }

		inline bool get_tick_ms(Tick &tick, const std::string &symbol, const uint64_t t_ms) noexcept {
            auto it = m_symbol_to_index.find(symbol);
			if (it == m_symbol_to_index.end()) return false;
			return m_symbol_db[it->second]->get_tick_ms(tick, t_ms);
        }

        inline bool get_next_tick_ms(Tick &tick, const size_t s_index, const uint64_t t_ms, const uint64_t t_ms_max) noexcept {
            return m_symbol_db[s_index]->get_next_tick_ms(tick, t_ms, t_ms_max);
        }

		inline bool get_next_tick_ms(Tick &tick, const std::string &symbol, const uint64_t t_ms, const uint64_t t_ms_max) noexcept {
            auto it = m_symbol_to_index.find(symbol);
			if (it == m_symbol_to_index.end()) return false;
			return m_symbol_db[it->second]->get_next_tick_ms(tick, t_ms, t_ms_max);
        }

		inline bool get_min_max_date(const bool use_tick_data, uint64_t &t_min, uint64_t &t_max) {
            t_min = 0;
            t_max = 0;
            bool is_error = false;
            for (auto &symbol : m_symbol_db) {
                uint64_t t_min_db = 0, t_max_db = 0;
                if (!symbol->get_min_max_date(use_tick_data, t_min_db, t_max_db)) {
                    is_error = true;
                    continue;
                }
                if (t_min == 0) t_min = t_min_db;
                if (t_max == 0) t_max = t_max_db;
                t_min = std::max(t_min_db, t_min);
                t_max = std::min(t_max_db, t_max);
            }
            return !is_error;
		}
	}; // QdbFxSymbolDB
};

#endif // TRADING_DB_QDB_FX_SYMBOLS_DB_HPP_INCLUDED
