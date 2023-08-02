#include <iostream>
#include <gtest/gtest.h>
#include "trading-db/tools/qdb/fx-symbols-db.hpp"

// Алгоритм расчета профита взят от сюда: https://www.mql5.com/ru/articles/10211

void test_eurusd_buy_sell() {

    trading_db::QdbFxSymbolDB::Config config;
    using SymbolConfig = trading_db::QdbFxSymbolDB::SymbolConfig;
    using TradeFxSignal = trading_db::QdbFxSymbolDB::TradeFxSignal;
    using TradeFxResult = trading_db::QdbFxSymbolDB::TradeFxResult;

    config.symbols = {
        SymbolConfig("AUDCAD"),
        SymbolConfig("AUDUSD"),
        SymbolConfig("EURUSD"),
        SymbolConfig("USDCAD"),
        SymbolConfig("USDJPY"),
    };
    config.path_db = "../../storage/test/";
    config.account_currency = "USD";
    config.account_leverage = 1;

    trading_db::QdbFxSymbolDB symbol_db;
    symbol_db.set_config(config);
    ASSERT_TRUE(symbol_db.init());

    trading_db::Tick open_tick;
    trading_db::Tick close_tick;
    const uint64_t open_time = ztime::get_timestamp(27,7,2023,0,0,0);
    const uint64_t close_time = ztime::get_timestamp(27,7,2023,2,0,0);
    const uint64_t open_delay = 5;
    const uint64_t close_delay = 5;
    ASSERT_TRUE(symbol_db.get_tick_ms(open_tick, "EURUSD", ztime::sec_to_ms(open_time+ open_delay)));
    ASSERT_TRUE(symbol_db.get_tick_ms(close_tick, "EURUSD", ztime::sec_to_ms(close_time + close_delay)));

    std::cout << "EURUSD "
        << ztime::get_str_date_time_ms(open_tick.t_ms / 1000.0)
        << " bid = " << open_tick.bid
        << " ask = " << open_tick.ask
        << std::endl;

    std::cout << "EURUSD "
        << ztime::get_str_date_time(close_tick.t_ms / 1000.0)
        << " bid = " << close_tick.bid
        << " ask = " << close_tick.ask
        << std::endl;

    // Проверяем BUY сделку
    {
        const double profit_buy = (close_tick.bid - open_tick.ask) * 100000.0;

        TradeFxSignal signal;
        signal.symbol           = "EURUSD";
        signal.lot_size         = 1.0;
        signal.open_date_ms     = ztime::sec_to_ms(open_time);
        signal.close_date_ms    = ztime::sec_to_ms(close_time);
        signal.open_delay_ms    = ztime::sec_to_ms(open_delay);;
        signal.close_delay_ms   = ztime::sec_to_ms(close_delay);;
        signal.duration_ms      = 0;
        signal.direction        = true;

        TradeFxResult result;
        EXPECT_TRUE(symbol_db.calc_trade_result(signal, result));

        std::cout << "BUY EURUSD "
            << "profit = " << result.profit
            << " open price  = " << result.open_price
            << " close price = " << result.close_price
            << std::endl;

        double tolerance = 0.00001;
        EXPECT_NEAR(result.profit, profit_buy, tolerance);
    }
    // SELL
    {
        const double profit_sell = (open_tick.bid - close_tick.ask) * 100000.0;

        TradeFxSignal signal;
        signal.symbol           = "EURUSD";
        signal.lot_size         = 1.0;
        signal.open_date_ms     = ztime::sec_to_ms(open_time);
        signal.close_date_ms    = ztime::sec_to_ms(close_time);
        signal.open_delay_ms    = ztime::sec_to_ms(open_delay);;
        signal.close_delay_ms   = ztime::sec_to_ms(close_delay);;
        signal.duration_ms      = 0;
        signal.direction        = false;

        TradeFxResult result;
        EXPECT_TRUE(symbol_db.calc_trade_result(signal, result));

        std::cout << "SELL EURUSD "
            << "profit = " << result.profit
            << " open price  = " << result.open_price
            << " close price = " << result.close_price
            << std::endl;

        double tolerance = 0.00001;
        EXPECT_NEAR(result.profit, profit_sell, tolerance);
    }
}

void test_usdcad_buy_sell() {

    trading_db::QdbFxSymbolDB::Config config;
    using SymbolConfig = trading_db::QdbFxSymbolDB::SymbolConfig;
    using TradeFxSignal = trading_db::QdbFxSymbolDB::TradeFxSignal;
    using TradeFxResult = trading_db::QdbFxSymbolDB::TradeFxResult;

    config.symbols = {
        SymbolConfig("AUDCAD"),
        SymbolConfig("AUDUSD"),
        SymbolConfig("EURUSD"),
        SymbolConfig("USDCAD"),
        SymbolConfig("USDJPY"),
    };
    config.path_db = "../../storage/test/";
    config.account_currency = "USD";
    config.account_leverage = 1;

    trading_db::QdbFxSymbolDB symbol_db;
    symbol_db.set_config(config);
    ASSERT_TRUE(symbol_db.init());

    trading_db::Tick open_tick;
    trading_db::Tick close_tick;
    const uint64_t open_time = ztime::get_timestamp(27,7,2023,0,0,0);
    const uint64_t close_time = ztime::get_timestamp(27,7,2023,2,0,0);
    const uint64_t open_delay = 5;
    const uint64_t close_delay = 5;
    ASSERT_TRUE(symbol_db.get_tick_ms(open_tick, "USDCAD", ztime::sec_to_ms(open_time+ open_delay)));
    ASSERT_TRUE(symbol_db.get_tick_ms(close_tick, "USDCAD", ztime::sec_to_ms(close_time + close_delay)));

    std::cout << "USDCAD "
        << ztime::get_str_date_time_ms(open_tick.t_ms / 1000.0)
        << " bid = " << open_tick.bid
        << " ask = " << open_tick.ask
        << std::endl;

    std::cout << "USDCAD "
        << ztime::get_str_date_time(close_tick.t_ms / 1000.0)
        << " bid = " << close_tick.bid
        << " ask = " << close_tick.ask
        << std::endl;

    // Проверяем BUY сделку
    {
        const double profit_buy = (close_tick.bid - open_tick.ask) * 100000.0 / close_tick.ask;

        TradeFxSignal signal;
        signal.symbol           = "USDCAD";
        signal.lot_size         = 1.0;
        signal.open_date_ms     = ztime::sec_to_ms(open_time);
        signal.close_date_ms    = ztime::sec_to_ms(close_time);
        signal.open_delay_ms    = ztime::sec_to_ms(open_delay);;
        signal.close_delay_ms   = ztime::sec_to_ms(close_delay);;
        signal.duration_ms      = 0;
        signal.direction        = true;

        TradeFxResult result;
        EXPECT_TRUE(symbol_db.calc_trade_result(signal, result));

        std::cout << "BUY USDCAD "
            << "profit = " << result.profit
            << " open price  = " << result.open_price
            << " close price = " << result.close_price
            << std::endl;

        double tolerance = 0.00001;
        EXPECT_NEAR(result.profit, profit_buy, tolerance);
    }
    // SELL
    {
        const double profit_sell = (open_tick.bid - close_tick.ask) * 100000.0 / close_tick.ask;

        TradeFxSignal signal;
        signal.symbol           = "USDCAD";
        signal.lot_size         = 1.0;
        signal.open_date_ms     = ztime::sec_to_ms(open_time);
        signal.close_date_ms    = ztime::sec_to_ms(close_time);
        signal.open_delay_ms    = ztime::sec_to_ms(open_delay);;
        signal.close_delay_ms   = ztime::sec_to_ms(close_delay);;
        signal.duration_ms      = 0;
        signal.direction        = false;

        TradeFxResult result;
        EXPECT_TRUE(symbol_db.calc_trade_result(signal, result));

        std::cout << "SELL USDCAD "
            << "profit = " << result.profit
            << " open price  = " << result.open_price
            << " close price = " << result.close_price
            << std::endl;

        double tolerance = 0.00001;
        EXPECT_NEAR(result.profit, profit_sell, tolerance);
    }
}

void test_audcad_buy_sell() {

    trading_db::QdbFxSymbolDB::Config config;
    using SymbolConfig = trading_db::QdbFxSymbolDB::SymbolConfig;
    using TradeFxSignal = trading_db::QdbFxSymbolDB::TradeFxSignal;
    using TradeFxResult = trading_db::QdbFxSymbolDB::TradeFxResult;

    config.symbols = {
        SymbolConfig("AUDCAD"),
        SymbolConfig("AUDUSD"),
        SymbolConfig("EURUSD"),
        SymbolConfig("USDCAD"),
        SymbolConfig("USDJPY"),
    };
    config.path_db = "../../storage/test/";
    config.account_currency = "USD";
    config.account_leverage = 1;

    trading_db::QdbFxSymbolDB symbol_db;
    symbol_db.set_config(config);
    ASSERT_TRUE(symbol_db.init());

    trading_db::Tick open_tick;
    trading_db::Tick close_tick;
    trading_db::Tick last_tick;

    const uint64_t open_time = ztime::get_timestamp(27,7,2023,0,0,0);
    const uint64_t close_time = ztime::get_timestamp(27,7,2023,2,0,0);
    const uint64_t open_delay = 5;
    const uint64_t close_delay = 5;

    ASSERT_TRUE(symbol_db.get_tick_ms(open_tick, "AUDCAD", ztime::sec_to_ms(open_time+ open_delay)));
    ASSERT_TRUE(symbol_db.get_tick_ms(close_tick, "AUDCAD", ztime::sec_to_ms(close_time + close_delay)));
    ASSERT_TRUE(symbol_db.get_tick_ms(last_tick, "USDCAD", ztime::sec_to_ms(close_time + close_delay)));

    std::cout << "AUDCAD "
        << ztime::get_str_date_time_ms(open_tick.t_ms / 1000.0)
        << " bid = " << open_tick.bid
        << " ask = " << open_tick.ask
        << std::endl;

    std::cout << "AUDCAD "
        << ztime::get_str_date_time(close_tick.t_ms / 1000.0)
        << " bid = " << close_tick.bid
        << " ask = " << close_tick.ask
        << std::endl;

    // Проверяем BUY сделку
    {
        const double profit_buy = (close_tick.bid - open_tick.ask) * 100000.0 / last_tick.ask;

        TradeFxSignal signal;
        signal.symbol           = "AUDCAD";
        signal.lot_size         = 1.0;
        signal.open_date_ms     = ztime::sec_to_ms(open_time);
        signal.close_date_ms    = ztime::sec_to_ms(close_time);
        signal.open_delay_ms    = ztime::sec_to_ms(open_delay);;
        signal.close_delay_ms   = ztime::sec_to_ms(close_delay);;
        signal.duration_ms      = 0;
        signal.direction        = true;

        TradeFxResult result;
        EXPECT_TRUE(symbol_db.calc_trade_result(signal, result));

        std::cout << "BUY AUDCAD "
            << "profit = " << result.profit
            << " open price  = " << result.open_price
            << " close price = " << result.close_price
            << std::endl;

        double tolerance = 0.00001;
        EXPECT_NEAR(result.profit, profit_buy, tolerance);
    }
    // SELL
    {
        const double profit_sell = (open_tick.bid - close_tick.ask) * 100000.0 / last_tick.ask;

        TradeFxSignal signal;
        signal.symbol           = "AUDCAD";
        signal.lot_size         = 1.0;
        signal.open_date_ms     = ztime::sec_to_ms(open_time);
        signal.close_date_ms    = ztime::sec_to_ms(close_time);
        signal.open_delay_ms    = ztime::sec_to_ms(open_delay);;
        signal.close_delay_ms   = ztime::sec_to_ms(close_delay);;
        signal.duration_ms      = 0;
        signal.direction        = false;

        TradeFxResult result;
        EXPECT_TRUE(symbol_db.calc_trade_result(signal, result));

        std::cout << "SELL AUDCAD "
            << "profit = " << result.profit
            << " open price  = " << result.open_price
            << " close price = " << result.close_price
            << std::endl;

        double tolerance = 0.00001;
        EXPECT_NEAR(result.profit, profit_sell, tolerance);
    }
}

void test_audnzd_buy_sell() {

    trading_db::QdbFxSymbolDB::Config config;
    using SymbolConfig = trading_db::QdbFxSymbolDB::SymbolConfig;
    using TradeFxSignal = trading_db::QdbFxSymbolDB::TradeFxSignal;
    using TradeFxResult = trading_db::QdbFxSymbolDB::TradeFxResult;

    config.symbols = {
        SymbolConfig("AUDCAD"),
        SymbolConfig("AUDUSD"),
        SymbolConfig("EURUSD"),
        SymbolConfig("USDCAD"),
        SymbolConfig("USDJPY"),
        SymbolConfig("AUDNZD"),
        SymbolConfig("NZDUSD"),
    };
    config.path_db = "../../storage/test/";
    config.account_currency = "USD";
    config.account_leverage = 1;

    trading_db::QdbFxSymbolDB symbol_db;
    symbol_db.set_config(config);
    ASSERT_TRUE(symbol_db.init());

    trading_db::Tick open_tick;
    trading_db::Tick close_tick;
    trading_db::Tick last_tick;

    const uint64_t open_time = ztime::get_timestamp(27,7,2023,0,0,0);
    const uint64_t close_time = ztime::get_timestamp(27,7,2023,2,0,0);
    const uint64_t open_delay = 5;
    const uint64_t close_delay = 5;

    ASSERT_TRUE(symbol_db.get_tick_ms(open_tick, "AUDNZD", ztime::sec_to_ms(open_time+ open_delay)));
    ASSERT_TRUE(symbol_db.get_tick_ms(close_tick, "AUDNZD", ztime::sec_to_ms(close_time + close_delay)));
    ASSERT_TRUE(symbol_db.get_tick_ms(last_tick, "NZDUSD", ztime::sec_to_ms(close_time + close_delay)));

    std::cout << "AUDNZD "
        << ztime::get_str_date_time_ms(open_tick.t_ms / 1000.0)
        << " bid = " << open_tick.bid
        << " ask = " << open_tick.ask
        << std::endl;
    std::cout << "AUDNZD "
        << ztime::get_str_date_time(close_tick.t_ms / 1000.0)
        << " bid = " << close_tick.bid
        << " ask = " << close_tick.ask
        << std::endl;

    // Проверяем BUY сделку
    {
        const double profit_buy = (close_tick.bid - open_tick.ask) * 100000.0 * last_tick.bid;

        TradeFxSignal signal;
        signal.symbol           = "AUDNZD";
        signal.lot_size         = 1.0;
        signal.open_date_ms     = ztime::sec_to_ms(open_time);
        signal.close_date_ms    = ztime::sec_to_ms(close_time);
        signal.open_delay_ms    = ztime::sec_to_ms(open_delay);;
        signal.close_delay_ms   = ztime::sec_to_ms(close_delay);;
        signal.duration_ms      = 0;
        signal.direction        = true;

        TradeFxResult result;
        EXPECT_TRUE(symbol_db.calc_trade_result(signal, result));

        std::cout << "BUY AUDNZD "
            << "profit = " << result.profit
            << " open price  = " << result.open_price
            << " close price = " << result.close_price
            << std::endl;

        double tolerance = 0.00001;
        EXPECT_NEAR(result.profit, profit_buy, tolerance);
    }
    // SELL
    {
        const double profit_sell = (open_tick.bid - close_tick.ask) * 100000.0 * last_tick.bid;

        TradeFxSignal signal;
        signal.symbol           = "AUDNZD";
        signal.lot_size         = 1.0;
        signal.open_date_ms     = ztime::sec_to_ms(open_time);
        signal.close_date_ms    = ztime::sec_to_ms(close_time);
        signal.open_delay_ms    = ztime::sec_to_ms(open_delay);;
        signal.close_delay_ms   = ztime::sec_to_ms(close_delay);;
        signal.duration_ms      = 0;
        signal.direction        = false;

        TradeFxResult result;
        EXPECT_TRUE(symbol_db.calc_trade_result(signal, result));

        std::cout << "SELL AUDNZD "
            << "profit = " << result.profit
            << " open price  = " << result.open_price
            << " close price = " << result.close_price
            << std::endl;

        double tolerance = 0.00001;
        EXPECT_NEAR(result.profit, profit_sell, tolerance);
    }
}

TEST(test_major, test_calc_trade_result) {
    test_eurusd_buy_sell();
    test_usdcad_buy_sell();
}

TEST(test_cross, test_calc_trade_result) {
    test_audcad_buy_sell();
    test_audnzd_buy_sell();
}

int main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
