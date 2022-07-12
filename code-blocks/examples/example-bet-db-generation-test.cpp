#include <iostream>
#include <random>
#include <trading-db/bet-database.hpp>

int main() {
    std::cout << "Hello world!" << std::endl;
    const std::string path("storage/example-bet-db-test.db");
    std::cout << "#generation" << std::endl << std::endl;
    {
        trading_db::BetDatabase bet_db;
        bet_db.config.use_log = true;

        std::cout << bet_db.open(path) << std::endl;

        std::cout << "#remove_all" << std::endl;
        std::cout << bet_db.remove_all() << std::endl;

        using bet_t = trading_db::BetDatabase::BetData;

        std::random_device dev;
        std::mt19937 rng(dev());
        std::uniform_int_distribution<int> dist_amount(1,700);
        std::uniform_int_distribution<int> dist_broker(0,1);
        std::uniform_int_distribution<int> dist_timestamp(60,24*3600);
        //std::uniform_int_distribution<int> dist_timestamp(24*3600,24*3600*30);
        std::uniform_int_distribution<int> dist_duration(1,30);
        std::uniform_int_distribution<int> dist_contract_type(0,1);
        std::uniform_int_distribution<int> dist_currency(0, 1);
        std::uniform_int_distribution<int> dist_winrate(1,1000);
        std::uniform_int_distribution<int> dist_ping(150,5000);
        std::uniform_int_distribution<int> dist_signal(0,2);
        std::uniform_int_distribution<int> dist_symbol(0,9);
        std::uniform_real_distribution<double> dist_price(1.0, 2.0);
        std::uniform_int_distribution<int> dist_payout(60, 85);
        std::uniform_int_distribution<int> dist_demo(0,1);

        ztime::timestamp_t timestamp = ztime::get_timestamp(01,01,2021,0,0,0);

        std::cout << "#fill data" << std::endl;
        std::vector<bet_t> bets;
        for (size_t i = 0; i < 10000; ++i) {
            bet_t bet;
            bet.amount = dist_amount(rng);
            bet.broker_id = i + 123457;
            const int b = dist_broker(rng);
            if (b == 0) bet.broker = "Intrade Bar";
            else bet.broker = "Binary.com";

            timestamp += dist_timestamp(rng);

            bet.open_date = 1000*timestamp;
            bet.duration = 60 * dist_duration(rng);
            bet.close_date = 1000*(timestamp + bet.duration);

            bet.comment = "test";

            if (dist_contract_type(rng) == 0) {
                bet.contract_type = trading_db::BetDatabase::ContractType::BUY;
            } else {
                bet.contract_type = trading_db::BetDatabase::ContractType::SELL;
            }

            if (dist_currency(rng) == 0) {
                bet.currency = "USD";
            } else {
                bet.currency = "RUB";
            }

            const int signal = dist_signal(rng);
            const int tw = signal == 0 ? 70 : signal == 1 ? 80 : 84;

            const int w = dist_winrate(rng);
            if (w < 2) {
                bet.status = trading_db::BetDatabase::BetStatus::STANDOFF;
            } else
            if (w < (10*tw)) {
                bet.status = trading_db::BetDatabase::BetStatus::WIN;
            } else {
                bet.status = trading_db::BetDatabase::BetStatus::LOSS;
            }

            bet.ping = dist_ping(rng);
            bet.delay = bet.ping - 50;

            bet.open_price  = dist_price(rng);
            if (w < 3) {
                bet.close_price = bet.open_price;
            } else
            if (w < tw) {
                if (bet.contract_type == trading_db::BetDatabase::ContractType::BUY) bet.close_price = bet.open_price + 0.00005;
                else bet.close_price = bet.open_price - 0.00005;
            } else {
                if (bet.contract_type == trading_db::BetDatabase::ContractType::BUY) bet.close_price = bet.open_price - 0.00005;
                else bet.close_price = bet.open_price + 0.00005;
            }

            bet.payout = (double)dist_payout(rng) / 100.d;
            bet.profit = bet.amount * bet.payout;
            bet.signal = "test-" + std::to_string(signal);
            bet.step = 0;

            const int symbol = dist_symbol(rng);

            switch (symbol) {
            case 0:
                bet.symbol = "EURUSD";
                break;
            case 1:
                bet.symbol = "EURCAD";
                break;
            case 2:
                bet.symbol = "EURGBP";
                break;
            case 3:
                bet.symbol = "AUDUSD";
                break;
            case 4:
                bet.symbol = "AUDCAD";
                break;
            case 5:
                bet.symbol = "AUDCHF";
                break;
            case 6:
                bet.symbol = "GBPCAD";
                break;
            case 7:
                bet.symbol = "GBPCHF";
                break;
            case 8:
                bet.symbol = "USDJPY";
                break;
            case 9:
                bet.symbol = "USDCHF";
                break;
            default:
                bet.symbol = "EURUSD";
                break;
            };

            bet.demo = dist_demo(rng) == 1;

            bet.type = trading_db::BetDatabase::BoType::SPRINT;
            bet.user_data = "test";
            bet.uid = bet_db.get_bet_uid();

            bets.push_back(bet);
        }

        std::cout << "#replace" << std::endl;
        for (size_t i = 0; i < bets.size(); ++i) {
            bet_db.replace_bet(bets[i]);
        }
        std::cout << "#flush" << std::endl;
        bet_db.flush();

        std::cout << "#get_bets" << std::endl;

        trading_db::BetDatabase::RequestConfig req_config;
        req_config.use_real = true;
        req_config.use_demo = true;
        std::vector<bet_t> read_bets = bet_db.get_bets<std::vector<bet_t>>(req_config);

        if (0)
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

        std::cout << "#meta_bets" << std::endl;
        trading_db::BetDatabase::BetMetaStats meta_bets;
        meta_bets.calc(read_bets);

        std::cout << "#currencies" << std::endl;
        for (auto &c : meta_bets.currencies) {
            std::cout << c << std::endl;
        }

        std::cout << "#brokers" << std::endl;
        for (auto &b : meta_bets.brokers) {
            std::cout << b << std::endl;
        }

        std::cout << "#signals" << std::endl;
        for (auto &b : meta_bets.signals) {
            std::cout << b << std::endl;
        }

        std::cout << "#demo" << std::endl;
        std::cout << meta_bets.demo << std::endl;
        std::cout << "#real" << std::endl;
        std::cout << meta_bets.real << std::endl;

        std::cout << "#stats_bet" << std::endl;
        trading_db::BetDatabase::BetStats bet_stats;
        bet_stats.calc(read_bets,1000);
        std::cout << "#bet_stats.total_gain " << bet_stats.total_gain << std::endl;
        std::cout << "#bet_stats.total_profit " << bet_stats.total_profit << std::endl;
        std::cout << "#aver_absolute_profit_per_trade " << bet_stats.aver_absolute_profit_per_trade << std::endl;
        std::cout << "#z-score = " << bet_stats.z_score.value << std::endl;
        std::cout << "#z-score total trades  = " << bet_stats.z_score.total_trades << std::endl;
        std::cout << "#end" << std::endl;
    }
    std::cout << "#read" << std::endl << std::endl;
    {
        trading_db::BetDatabase bet_db;
        bet_db.config.use_log = true;

        std::cout << bet_db.open(path) << std::endl;

        using bet_t = trading_db::BetDatabase::BetData;

        std::cout << "#get_bets" << std::endl;

        trading_db::BetDatabase::RequestConfig req_config;
        req_config.use_real = true;
        req_config.use_demo = true;
        std::vector<bet_t> read_bets = bet_db.get_bets<std::vector<bet_t>>(req_config);

        if (0)
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

        std::cout << "#meta_bets" << std::endl;
        trading_db::BetDatabase::BetMetaStats meta_bets;
        meta_bets.calc(read_bets);

        std::cout << "#currencies" << std::endl;
        for (auto &c : meta_bets.currencies) {
            std::cout << c << std::endl;
        }

        std::cout << "#brokers" << std::endl;
        for (auto &b : meta_bets.brokers) {
            std::cout << b << std::endl;
        }

        std::cout << "#signals" << std::endl;
        for (auto &b : meta_bets.signals) {
            std::cout << b << std::endl;
        }

        std::cout << "#demo" << std::endl;
        std::cout << meta_bets.demo << std::endl;
        std::cout << "#real" << std::endl;
        std::cout << meta_bets.real << std::endl;

        std::cout << "#stats_bet" << std::endl;
        trading_db::BetDatabase::BetStats bet_stats;
        bet_stats.calc(read_bets,1000);
        std::cout << "#bet_stats.total_gain " << bet_stats.total_gain << std::endl;
        std::cout << "#bet_stats.total_profit " << bet_stats.total_profit << std::endl;
        std::cout << "#aver_absolute_profit_per_trade " << bet_stats.aver_absolute_profit_per_trade << std::endl;
        std::cout << "#z-score = " << bet_stats.z_score.value << std::endl;
        std::cout << "#z-score total trades  = " << bet_stats.z_score.total_trades << std::endl;
        std::cout << "#end" << std::endl;
    }
    std::cout << "#exit" << std::endl;
    return 0;
}
