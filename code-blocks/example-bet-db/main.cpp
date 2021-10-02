#include <iostream>
#include "../../include/trading-db/bet-database.hpp"

int main() {
    std::cout << "Hello world!" << std::endl;
    const std::string path("bets.db");
    trading_db::BetDatabase db(path);

    std::cout << "write" << std::endl;
    for (int  i = 0; i < 1000; ++i) {
        trading_db::BetDatabase::BetData bet;

        // заполняем структуру ставки
        bet.open_date = xtime::get_ftimestamp(1,1,2020,0,0,0) + i * 60;
        bet.close_date = xtime::get_ftimestamp(1,1,2020,5,0,0) + i * 60 + 180;
        bet.open_price = 1.15698;
        bet.close_price = 1.15618;

        bet.amount = 100;
        bet.profit = 80;
        bet.payout = 0.8;

        bet.broker_id = i + 10012;
        bet.duration = 180;
        bet.delay = 0.01;

        bet.status = i % 2 == 0 ? trading_db::BetDatabase::BetStatus::WIN : trading_db::BetDatabase::BetStatus::LOSS;
        bet.contract_type = trading_db::BetDatabase::ContractTypes::SELL;
        bet.type = trading_db::BetDatabase::BoTypes::SPRINT;

        bet.symbol = "EURUSD";
        bet.broker = "intrade.bar";
        bet.currency = "USD";
        bet.signal = "test";
        bet.comment = "test bo";
        bet.demo = false;

        // добавляем ставку в БД
        db.add(bet);
    }

    return 0;
}
