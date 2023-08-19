#include <iostream>
#include "trading-db/tools/qdb/fx-history.hpp"
#include <gtest/gtest.h>

void test_all() {
    trading_db::QdbFxHistoryV1          fx_history;

    trading_db::QdbFxHistoryV1::Config  config;
    using SymbolConfig = trading_db::QdbFxHistoryV1::SymbolConfig;
    using TradeFxSignal = trading_db::QdbFxHistoryV1::TradeFxSignal;
    using TradeFxResult = trading_db::QdbFxHistoryV1::TradeFxResult;

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

    //{ настройка для теста на истории

    // период перед началом теста
    config.pre_start_period = 7*ztime::SEC_PER_DAY;
    // дата теста
    config.start_date   = ztime::get_timestamp(10,7,2023,0,0,0);
    config.stop_date    = ztime::get_timestamp(28,7,2023,0,0,0);
    // период опроса тиков на участах времени торговли
    config.tick_period  = 1.0;
    // таймфрейм баров
    config.timeframe    = 60;
    // режим "новый тик" на участах времени торговли
    config.use_new_tick_mode    = false;
    // время торговли
    config.add_trade_period(trading_db::TimePeriod(trading_db::TimePoint(10, 15, 0), trading_db::TimePoint(10, 15, 5), 1));
    config.add_trade_period(trading_db::TimePeriod(trading_db::TimePoint(11, 15, 0), trading_db::TimePoint(11, 15, 5), 2));
    config.add_trade_period(trading_db::TimePeriod(trading_db::TimePoint(0, 0, 0), trading_db::TimePoint(0, 0, 0), 3));
    //}

    //{ настройка обратных вызовов

    // выводим сообщение об ошибке и т.д.
    config.on_msg = [](const std::string &msg) {
        TRADING_DB_PRINT << "history: " << msg << std::endl;
    };

    // выводим сообщение о завершении теста на символе
    config.on_end_test_symbol = [&config](const size_t t_index, const size_t s_index) {
        TRADING_DB_PRINT << "history: finished history on symbol " << config.symbols[s_index].symbol << std::endl;
    };

    // выводим сообщение о завершении работы потока
    config.on_end_test_thread = [](const size_t i, const size_t n) {
        TRADING_DB_PRINT << "history: finished history on thread " << i << " / " << n << std::endl;
    };

    // выводим сообщение о завершении теста
    config.on_end_test = []() {
        TRADING_DB_PRINT << "history: test completed!" << std::endl;
    };

    // выводим сообщение о дате теста
    config.on_date_msg = [&config](const size_t t_index, const size_t s_index, const uint64_t t_ms) {
        //TRADING_DB_PRINT << "history: " << config.symbols[s_index].symbol << " date " << ztime::get_str_date(ztime::ms_to_sec(t_ms)) << std::endl;
        //TRADING_DB_PRINT << t_index << " on_date_msg: " << s_index << " date " << ztime::get_str_date(ztime::ms_to_sec(t_ms)) << std::endl;
    };

    // используем все символы
    config.on_symbol = [](const size_t s_index) -> bool {
        return true;
    };

    // событьие получение нового бара
    config.on_candle = [&config](
            const size_t                t_index,
            const size_t                s_index,    // Номер символа
            const uint64_t              t_ms,       // Время тестера
            const std::set<int32_t>     &period_id, // Флаг периода теста
            const trading_db::Candle    &candle     // Данные бара
            ) {
        std::string str_period;
        for (auto &item : period_id) {
            str_period += std::to_string(item) + ";";
        }
        //TRADING_DB_PRINT << t_index << " on_candle: " << config.symbols[s_index].symbol << "; c: " << candle.close << "; p: " << str_period << "; t: " << ztime::get_str_date_time(candle.timestamp) << std::endl;
    };

    config.on_tick = [&config](
            const size_t                t_index,
            const size_t                s_index,    // Номер символа
            const uint64_t              t_ms,       // Время тестера
            const std::set<int32_t>     &period_id, // Флаг периода теста
            const trading_db::Tick      &tick       // Данные тика
            ) {
        std::string str_period;
        for (auto &item : period_id) {
            str_period += std::to_string(item) + ";";
        }
        //TRADING_DB_PRINT << t_index << " on_tick: " << config.symbols[s_index].symbol << "; bid: " << tick.bid << "; p: " << str_period << "; t: " << ztime::get_str_time((double)t_ms/1000.0) << std::endl;
    };

    config.on_test = [&config, &fx_history](
            const size_t                t_index,
            const size_t                s_index,    // Номер символа
            const uint64_t              t_ms,       // Время тестера
            const std::set<int32_t>     &period_id  // Флаг периода теста
            ) {
        std::string str_period;
        for (auto &item : period_id) {
            str_period += std::to_string(item) + ";";
        }
        TRADING_DB_PRINT << t_index << " on_test: " << config.symbols[s_index].symbol << " p: " << str_period << " t: " << ztime::get_str_time(ztime::ms_to_sec(t_ms)) << std::endl;

        if (period_id.find(3) != period_id.end()) {
            if (ztime::ms_to_sec(t_ms) == ztime::get_timestamp(27,7,2023,0,0,0)) {
                const double profit_buy     = -102.41;
                const uint64_t open_delay   = 5;
                const uint64_t close_delay  = 5;

                TradeFxSignal signal;
                signal.symbol           = "AUDNZD";
                signal.lot_size         = 1.0;
                signal.open_date_ms     = t_ms;
                signal.close_date_ms    = t_ms + ztime::sec_to_ms(2 * ztime::SEC_PER_HOUR);
                signal.open_delay_ms    = ztime::sec_to_ms(open_delay);;
                signal.close_delay_ms   = ztime::sec_to_ms(close_delay);;
                signal.duration_ms      = 0;
                signal.direction        = true;

                TradeFxResult result;
                EXPECT_TRUE(fx_history.calc_trade_result(signal, result));


                TRADING_DB_PRINT << "BUY AUDNZD "
                    << "profit = " << result.profit
                    << " open price  = " << result.open_price
                    << " close price = " << result.close_price
                    << " t = " << ztime::get_str_date_time_ms(t_ms / 1000.0)
                    << std::endl;

                double tolerance = 0.01;
                EXPECT_NEAR(result.profit, profit_buy, tolerance);
            }
        }
    };

    //}

    fx_history.set_config(config);
    ASSERT_TRUE(fx_history.init());
    fx_history.start(trading_db::QDB_HISTORY_TEST_MODE::SEGMENT);
}

TEST(TestCallback, TestQdbFxHistoryV1) {
    test_all();
}

int main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
