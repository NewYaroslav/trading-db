#pragma once
#ifndef TRADING_DB_QDB_ENUMS_HPP_INCLUDED
#define TRADING_DB_QDB_ENUMS_HPP_INCLUDED

namespace trading_db {

	/// Режимы использования цены
	enum class QDB_PRICE_MODE {
		BID_PRICE,
		ASK_PRICE,
		AVG_PRICE
	};

	/// Режим получения данных свечи
	enum class QDB_CANDLE_MODE {
		SRC_TICK,
		SRC_CANDLE
	};

	/// Таймфреймы
	enum class QDB_TIMEFRAMES {
		PERIOD_M1	= 1,
		PERIOD_M5	= 5,
		PERIOD_M15	= 15,
		PERIOD_M30	= 30,
		PERIOD_H1	= 60,
		PERIOD_H4	= 240,
		PERIOD_D1	= 1440,
	};

    enum class QDB_HISTORY_TEST_MODE {
        SYMBOL,
        SEGMENT,
    };

};

#endif // TRADING_DB_QDB_ENUMS_HPP_INCLUDED
