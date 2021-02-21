#include <iostream>
#include "../../include/bet-db.hpp"
//#include <sqlite_orm/sqlite_orm.h>
#include <xtime.hpp>

int main() {
    std::cout << "Hello world!" << std::endl;
    const std::string path("bets.db");
    trading_db::BetDB bet_db(path);

    std::cout << "server: " << bet_db.get_note("server") << std::endl;

    bet_db.clear();

    std::cout << "write" << std::endl;
    for (int  i = 0; i < 1000; ++i) {
        trading_db::BetDB::BetConfig bet;

        /* заполняем структуру ставки */
        bet.open_date = xtime::get_ftimestamp(1,1,2020,5,0,0) + i * 60;
        bet.close_date = 0;//xtime::get_ftimestamp(1,1,2020,5,0,0) + i * 60 + 180;
        bet.open_price = 1.15698;
        bet.close_price = 1.15618;

        bet.amount = 100;
        bet.profit = 80;
        bet.payout = 0.8;

        bet.broker_id = i + 10012;
        bet.duration = 180;
        bet.delay = 0.01;

        bet.status = i % 2 == 0 ? trading_db::BetDB::BetStatus::WIN : trading_db::BetDB::BetStatus::LOSS;
        bet.contract_type = trading_db::BetDB::BetContractType::SELL;
        bet.type = trading_db::BetDB::BetType::SPRINT;

        bet.symbol = "EURUSD";
        bet.broker = "intrade.bar";
        bet.currency = "USD";
        bet.signal = "test";
        bet.comment = "test bo";
        bet.demo = false;

        /* добавляем ставку в БД */
        bet_db.add(bet);
    }

    /* читаем данные за определенный период */
    std::cout << "read" << std::endl;
    std::vector<trading_db::BetDB::BetConfig> bets = bet_db.get_array(
        xtime::get_ftimestamp(1,1,2020,5,0,0),
        xtime::get_ftimestamp(1,1,2020,5,30,0));

    std::cout << "bets: " << bets.size() << std::endl;

    for(size_t i = 0; i < bets.size(); ++i) {
        std::cout << bets[i].id << " bet broker id: " << bets[i].broker_id << std::endl;
        std::cout << bets[i].symbol << std::endl;
        std::cout << xtime::get_str_date_time(bets[i].open_date) << std::endl;
        std::cout << xtime::get_str_date_time(bets[i].close_date) << std::endl;
        std::cout << bets[i].duration << std::endl;
        std::cout << bets[i].broker << std::endl;
    }

    bet_db.set_note("server", "pc");
    std::cout << "server: " << bet_db.get_note("server") << std::endl;

    std::cout << "winrate: " << bet_db.get_winrate(bets) << std::endl;
    std::cout << "total profit: " << bet_db.get_total_profit(bets) << std::endl;
    return 0;
}
