#include <iostream>
#include <trading-db/bo-trades-db.hpp>

int main() {
    std::cout << "Hello world!" << std::endl;
    const std::string path("storage/example-bet-db.db");
    std::cout << "#test-1" << std::endl << std::endl;
    {
        trading_db::BoTradesDB bet_db;
        bet_db.config.use_log = true;

        std::cout << "#open: " << bet_db.open(path) << std::endl;

        std::system("pause");

        std::cout << "#remove_all: " << bet_db.remove_all() << std::endl;

        using bet_t = trading_db::BoTradesDB::BoResult;

        std::cout << "#fill data" << std::endl;
        std::vector<bet_t> bets;
        for (size_t i = 0; i < 1000; ++i) {
            bet_t bet;
            bet.amount = 100 + i/100;
            bet.broker_id = i;
            bet.broker = "TEST " + std::to_string((size_t)(i/100));
            bet.open_date = ztime::get_timestamp(01,01,2019,20,30,50) * 1000 + i * 60;
            bet.duration = 60 * (i / 100);
            bet.close_date = bet.open_date + bet.duration;
            bet.comment = "test";
            if (i % 2 == 0) bet.contract_type = trading_db::BoTradesDB::ContractType::BUY;
            else bet.contract_type = trading_db::BoTradesDB::ContractType::SELL;
            bet.currency = "USD";
            bet.delay = 50 + i * 10;
            bet.ping = 100 + i * 10;
            bet.open_price  = 1.56785;
            bet.close_price = 1.56789;
            if ((i + 1) % 3 == 0) bet.payout = 0.7;
            else bet.payout = 0.8;
            bet.profit = bet.amount * bet.payout;
            if ((i + 2) % 3 == 0) bet.signal = "test-1";
            else bet.signal = "test-2";
            if ((i + 3) % 3 == 0) bet.status = trading_db::BoTradesDB::BoStatus::WIN;
            else bet.status = trading_db::BoTradesDB::BoStatus::LOSS;
            bet.step = 0;
            if ((i + 4) % 3 == 0) bet.symbol = "EURCAD";
            else bet.symbol = "AUDCAD";
            bet.type = trading_db::BoTradesDB::BoType::SPRINT;
            bet.user_data = "12345";
            bet.uid = bet_db.get_trade_uid();

            bets.push_back(bet);
        }

        std::cout << "#replace" << std::endl;
        for (size_t i = 0; i < 1000; ++i) {
            bet_db.replace_trade(bets[i]);
        }
        std::cout << "#flush" << std::endl;
        bet_db.flush();

        std::system("pause");

        std::cout << "#get_trades" << std::endl;

        trading_db::BoTradesDB::RequestConfig req_config;
        //req_config.min_amount = 108;
        req_config.brokers = {"TEST 1", "TEST 11"};
        req_config.symbols = {"AUDCAD"};
        req_config.min_payout = 0.8;

        std::vector<bet_t> read_bets = bet_db.get_trades<std::vector<bet_t>>(req_config);

        for (auto bet : read_bets) {
            std::cout << "--------------------------------------" << std::endl;
            std::cout <<  "uid "             << bet.uid               << std::endl;
            std::cout <<  "broker_id "	     << bet.broker_id 	      << std::endl;
            std::cout <<  "open_date " 	     << bet.open_date 	      << std::endl;
            std::cout <<  "close_date " 	 << bet.close_date 	      << std::endl;
            std::cout <<  "open_price " 	 << bet.open_price 	      << std::endl;
            std::cout <<  "close_price "     << bet.close_price       << std::endl;
            std::cout <<  "amount " 		 << bet.amount 		      << std::endl;
            std::cout <<  "profit " 		 << bet.profit 		      << std::endl;
            std::cout <<  "payout " 		 << bet.payout 		      << std::endl;
            std::cout <<  "delay " 		     << bet.delay 		      << std::endl;
            std::cout <<  "ping " 		     << bet.ping 		      << std::endl;
            std::cout <<  "duration " 	     << bet.duration 	      << std::endl;
            std::cout <<  "step " 		     << bet.step 		      << std::endl;
            std::cout <<  "demo "		     << bet.demo 		      << std::endl;
            std::cout <<  "last "		     << bet.last 		      << std::endl;
            std::cout <<  "contract_type "   << static_cast<int>(bet.contract_type)     << std::endl;
            std::cout <<  "status "          << static_cast<int>(bet.status)            << std::endl;
            std::cout <<  "type " 	         << static_cast<int>(bet.type) 	          << std::endl;
            std::cout <<  "symbol "          << bet.symbol            << std::endl;
            std::cout <<  "broker "          << bet.broker            << std::endl;
            std::cout <<  "currency "        << bet.currency          << std::endl;
            std::cout <<  "signal "          << bet.signal            << std::endl;
            std::cout <<  "comment "         << bet.comment           << std::endl;
            std::cout <<  "user_data "       << bet.user_data         << std::endl;
            std::cout << std::endl;
        }
        std::cout << "read_bets size " << read_bets.size() << std::endl;

        std::cout << "#end" << std::endl;
        std::system("pause");
    }
    std::system("pause");
    std::cout << "#test-2" << std::endl << std::endl;
    {
        trading_db::BoTradesDB bet_db;
        bet_db.config.use_log = true;

        std::cout << bet_db.open(path) << std::endl;

        std::cout << "#remove_all" << std::endl;
        std::cout << bet_db.remove_all() << std::endl;

        using bet_t = trading_db::BoTradesDB::BoResult;

        std::cout << "#fill data" << std::endl;
        std::vector<bet_t> bets;
        for (size_t i = 0; i < 10000; ++i) {
            bet_t bet;
            bet.amount = 100 + i/100;
            bet.broker_id = i;
            bet.broker = "TEST " + std::to_string((size_t)(i/100));
            bet.open_date = ztime::get_timestamp(01,01,2019,20,30,50) * 1000 + i * 60;
            bet.duration = 60 * (i / 100);
            bet.close_date = bet.open_date + bet.duration;
            bet.comment = "test";
            if (i % 2 == 0) bet.contract_type = trading_db::BoTradesDB::ContractType::BUY;
            else bet.contract_type = trading_db::BoTradesDB::ContractType::SELL;
            bet.currency = "USD";
            bet.delay = 50 + i * 10;
            bet.ping = 100 + i * 10;
            bet.open_price  = 1.56785;
            bet.close_price = 1.56789;
            if ((i + 1) % 3 == 0) bet.payout = 0.7;
            else bet.payout = 0.8;
            bet.profit = bet.amount * bet.payout;
            if ((i + 2) % 3 == 0) bet.signal = "test-1";
            else bet.signal = "test-2";
            if ((i + 3) % 3 == 0) bet.status = trading_db::BoTradesDB::BoStatus::WIN;
            else bet.status = trading_db::BoTradesDB::BoStatus::LOSS;
            bet.step = 0;
            if ((i + 4) % 3 == 0) bet.symbol = "EURCAD";
            else bet.symbol = "AUDCAD";
            bet.type = trading_db::BoTradesDB::BoType::SPRINT;
            bet.user_data = "12345";
            bet.uid = bet_db.get_trade_uid();
            bets.push_back(bet);
        }

        trading_db::BoTradesDB::RequestConfig req_config;
        //req_config.min_amount = 108;
        req_config.brokers = {"TEST 1", "TEST 11"};
        req_config.symbols = {"AUDCAD"};
        req_config.min_payout = 0.8;

        std::cout << "#replace with read" << std::endl;
        // можно одновременно писать и читать
        for (size_t k = 0; k < 1000; ++k) {
            for (size_t i = 0; i < bets.size(); ++i) {
                bet_db.replace_trade(bets[i]);
                std::vector<bet_t> read_bets = bet_db.get_trades<std::vector<bet_t>>(req_config);
                std::cout << "read_bets size " << read_bets.size() << " i " << i << " k " << k << std::endl;
            }
        }

        std::cout << "#end" << std::endl;
    }
    std::cout << "#exit" << std::endl;
    return 0;
}
