#include <iostream>
#include "trading-db/tools/qdb-history.hpp"

int main(int argc, char* argv[]) {
	std::cout << "start" << std::endl;

	//--------------------------------------------------------------------------
	//{ Начальные данные и классы
	//--------------------------------------------------------------------------

	const std::vector<std::string> symbols = {
		"AUDCAD",//"AUDCHF","AUDJPY",
		//"AUDNZD","AUDUSD","CADJPY",
		//"EURAUD","EURCAD","EURCHF",
		//"EURGBP","EURJPY","EURUSD",
		//"GBPAUD","GBPCHF","GBPJPY",
		//"GBPNZD","NZDJPY","NZDUSD",
		//"USDCAD","USDCHF","USDJPY"
	};

	trading_db::QdbHistory			history;
	trading_db::QdbHistory::Config	history_config;

	history_config.symbols = symbols;

	//--------------------------------------------------------------------------
	//}
	//--------------------------------------------------------------------------

	//--------------------------------------------------------------------------
	//{ Пользовательские настройки
	//--------------------------------------------------------------------------

	{
		// время начала и конца тестирования (Начало 15 1 2019 - Конец 27 9 2022)
		history_config.set_date(false, 15,1,2022);
		history_config.set_date(true, 15,1,2023);

		history_config.timeframe	= 60;
		history_config.tick_period	= 1.0;
		history_config.use_new_tick_mode = true;

		history_config.path_db = "D:\\_repoz_trading\\mega-connector\\storage\\alpary-mt5-qdb";

		history_config.add_trade_period(trading_db::TimePeriod(trading_db::TimePoint(1, 10, 0), trading_db::TimePoint(1, 10, 0), 1));
		history_config.add_trade_period(trading_db::TimePeriod(trading_db::TimePoint(1, 15, 0), trading_db::TimePoint(1, 15, 59), 2));
		history_config.add_trade_period(trading_db::TimePeriod(trading_db::TimePoint(1, 15, 30), trading_db::TimePoint(1, 16, 59), 3));
		history_config.add_trade_period(trading_db::TimePeriod(trading_db::TimePoint(2, 15, 0), trading_db::TimePoint(2, 15, 59), 4));
		history_config.add_trade_period(trading_db::TimePeriod(trading_db::TimePoint(3, 15, 30), trading_db::TimePoint(3, 15, 59), 5));
	}

	//--------------------------------------------------------------------------
	//}
	//--------------------------------------------------------------------------

	//--------------------------------------------------------------------------
	//{ Инициализация настроек истории
	//--------------------------------------------------------------------------

	history_config.on_msg = [&](const std::string &msg) {
		TRADING_DB_PRINT << "history: " << msg << std::endl;
	};

	history_config.on_end_test_symbol = [&](const size_t s_index) {
		TRADING_DB_PRINT << "history: finished history on symbol " << history_config.symbols[s_index] << std::endl;
	};

	history_config.on_end_test_thread = [&](const size_t i, const size_t n) {
		TRADING_DB_PRINT << "history: finished history on thread " << i << " / " << n << std::endl;
	};

	history_config.on_end_test = [&]() {
		TRADING_DB_PRINT << "history: test completed!" << std::endl;
	};

	history_config.on_date_msg = [&](const size_t s_index, const uint64_t t_ms) {
		TRADING_DB_PRINT << "history: " << history_config.symbols[s_index] << " date " << ztime::get_str_date(t_ms / ztime::MILLISECONDS_IN_SECOND) << std::endl;
	};

	history_config.on_symbol = [&](const size_t s_index) -> bool {
		//if (s_index != 0) return false;
		return true;
	};

	history_config.on_candle = [&](
			const size_t				s_index,	// Номер символа
			const uint64_t				t_ms,		// Время тестера
			const std::set<int32_t>		&period_id, // Флаг периода теста
			const trading_db::Candle	&candle		// Данные бара
			) {
		std::string temp;
		for (auto &item : period_id) {
			temp += std::to_string(item) + ";";
		}
		TRADING_DB_PRINT << "on_candle: " << symbols[s_index] << " p: " << temp << " t: " << ztime::get_str_time(candle.timestamp) << std::endl;
	};

	history_config.on_tick = [&](
			const size_t				s_index,	// Номер символа
			const uint64_t				t_ms,		// Время тестера
			const std::set<int32_t>		&period_id, // Флаг периода теста
			const trading_db::Tick		&tick		// Данные тика
			) {
		std::string temp;
		for (auto &item : period_id) {
			temp += std::to_string(item) + ";";
		}
		TRADING_DB_PRINT << "on_tick: " << symbols[s_index] << " p: " << temp << " t: " << ztime::get_str_time((double)t_ms/1000.0) << std::endl;
	};

	history_config.on_test = [&](
			const size_t				s_index,	// Номер символа
			const uint64_t				t_ms,		// Время тестера
			const std::set<int32_t>		&period_id	// Флаг периода теста
			) {
		std::string temp;
		for (auto &item : period_id) {
			temp += std::to_string(item) + ";";
		}
		TRADING_DB_PRINT << "on_test: " << symbols[s_index] << " p: " << temp << " t: " << ztime::get_str_time((double)t_ms/1000.0) << std::endl;

		trading_db::QdbHistory::TradeBoSignal signal;
		signal.s_index	= s_index;
		signal.t_ms		= t_ms;
		signal.delay	= 2.0;
		signal.duration = 180;
		signal.up		= true;

		trading_db::QdbHistory::TradeBoResult result;

		if (history.check_trade_result(signal, result)) {
			TRADING_DB_PRINT
				<< "bo: "	<< symbols[s_index]
				<< " op: "	<< result.open_price
				<< " cp: "	<< result.close_price
				<< " t: "	<< ztime::get_str_time_ms(result.open_date)
				<< " -> "	<< ztime::get_str_time_ms(result.close_date)
				<< " r: "	<< result.win
				<< " ok: "	<< result.ok
				<< std::endl;
		} else {
			TRADING_DB_PRINT
				<< "error bo: " << symbols[s_index]
				<< " ok: "		<< result.ok
				<< std::endl;
		}
	};

	//--------------------------------------------------------------------------
	//}
	//--------------------------------------------------------------------------

	//--------------------------------------------------------------------------
	//{ Инициализация и запуск тестирования
	//--------------------------------------------------------------------------

	history.set_config(history_config);
	history.start();

	//--------------------------------------------------------------------------
	//}
	//--------------------------------------------------------------------------

	std::system("pause");
	return 0;
}
