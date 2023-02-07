#include <iostream>
#include "../../include/trading-db/bo-trades-db.hpp"

int main() {
    std::cout << "Hello world!" << std::endl;

    {
        const std::string path("-trades.db");
        trading_db::BoTradesDB db(path);
        std::cout << "destroy" << std::endl;
    }
    std::cout << "destroy ok" << std::endl;

    const std::string path("trades.db");
    trading_db::BoTradesDB db(path);

    std::cout << "write" << std::endl;
    for (int  i = 0; i < 1000; ++i) {
        trading_db::BoTradesDB::BoResult bo;

        // заполняем структуру ставки
        bo.open_date = ztime::get_ftimestamp(1,1,2020,0,0,0) + i * 60;
        bo.close_date = ztime::get_ftimestamp(1,1,2020,5,0,0) + i * 60 + 180;
        bo.open_price = 1.15698;
        bo.close_price = 1.15618;

        bo.amount = 100;
        bo.profit = 80;
        bo.payout = 0.8;

        bo.broker_id = i + 10012;
        bo.duration = 180;
        bo.delay = 0.01;

        bo.status = i % 2 == 0 ? trading_db::BoTradesDB::BoStatus::WIN : trading_db::BoTradesDB::BoStatus::LOSS;
        bo.contract_type = trading_db::BoTradesDB::ContractType::SELL;
        bo.type = trading_db::BoTradesDB::BoType::SPRINT;

        bo.symbol = "EURUSD";
        bo.broker = "intrade.bar";
        bo.currency = "USD";
        bo.signal = "test";
        bo.comment = "test bo";
        bo.demo = false;

        // добавляем ставку в БД
        db.replace_trade(bo);
    }

    return 0;
}
