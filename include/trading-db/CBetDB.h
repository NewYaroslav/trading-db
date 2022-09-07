#pragma once




#include <vector>
#include <algorithm>
#include <limits>
#include <map>
#include <utility>
#include <iostream>


//Disable all warnings (for VS only)
#pragma warning(disable : 4305)
#pragma warning(disable : 4244)
#pragma warning(disable : 4018)
#pragma warning(disable : 4305)

#include "xtime.hpp"
#include "sqlite_orm.h"
using namespace std;
using namespace _xtime;



enum BetStatus {
	UNKNOWN_STATE = 0,                      //< �������������� ���������
	OPENING_ERROR = -3,                     //< ������ ��������
	CHECK_ERROR = -4,                       //< ������ �������� ���������� ������
	WAITING_COMPLETION = -5,                //< �������� ���������� ������
	WIN = 1,                               	//< ������� ������
	LOSS = -1,                              //< ��������� ������
	STANDOFF = -2,							//< �����
};

enum BetContractType {
	BUY = 1,
	SELL = -1
};

struct Settings
{
	string key;
	string value;
};

struct BetConfig
{
	int		id = -1;
	double open_date; 		//-����� ������� �������� ������
	double close_date; 		//-����� ������� �������� ������
	double open_price;		//-���� ����� � ������
	double close_price;		//-���� ������ �� ������

	double amount; 			//-������ ������
	double profit;			//-������ �������
	double payout;			//-������� ������

	uint64_t broker_id;		//-���������� ����� ������, ������� ����������� ������
	uint32_t duration; 		//-����������(������������) ��������� ������� � ��������
	float delay; 			//-�������� �� �������� ������ � ��������

	int8_t _contract_type; 	//-��� ��������� ������ ��� ��, (+1) - ��� BUY, (-1) - ��� SELL
	int8_t _bet_status;		//-��������� ������ ������ ��� ��, ��.BetStatus
	//BetStatus 
	double bet_status;	//-��������� ������, ��.BetStatus
	//BetContractType 
	double contract_type; //-��� ���������

	string symbol;			//-��� �������(�������� ����, �����, ������ � ��., �������� EURUSD)
	string broker;			//-��� �������
	string currency;		//-������ ������
	string signal;			//-��� �������, ��������� ��� ����������, ������ ��� ��������� �������
	string type;			//-��� ��������� �������(SPRINT, CLASSIC � �.�., � ������ �������� ������, ����� ����� ��� ���� ������ ����)
	string comment;			//-�����������

	bool demo;				//-���� ���� ��������
};

inline auto init_storage(const std::string& path)
{
	return sqlite_orm::make_storage(path,
		sqlite_orm::make_table("BetConfig",
			sqlite_orm::make_column("id", &BetConfig::id, sqlite_orm::unique(), sqlite_orm::primary_key()),
			sqlite_orm::make_column("open_date", &BetConfig::open_date),
			sqlite_orm::make_column("open_price", &BetConfig::open_price),
			sqlite_orm::make_column("close_price", &BetConfig::close_price),
			sqlite_orm::make_column("amount", &BetConfig::amount),
			sqlite_orm::make_column("profit", &BetConfig::profit),
			sqlite_orm::make_column("payout", &BetConfig::payout),
			sqlite_orm::make_column("broker_id", &BetConfig::broker_id),
			sqlite_orm::make_column("duration", &BetConfig::duration),
			sqlite_orm::make_column("delay", &BetConfig::delay),
			sqlite_orm::make_column("contract_type", &BetConfig::contract_type),
			sqlite_orm::make_column("bet_status", &BetConfig::bet_status),
			sqlite_orm::make_column("symbol", &BetConfig::symbol),
			sqlite_orm::make_column("broker", &BetConfig::broker),
			sqlite_orm::make_column("currency", &BetConfig::currency),
			sqlite_orm::make_column("signal", &BetConfig::signal),
			sqlite_orm::make_column("type", &BetConfig::type),
			sqlite_orm::make_column("comment", &BetConfig::comment),
			sqlite_orm::make_column("demo", &BetConfig::demo)),
		sqlite_orm::make_table("Settings",
			sqlite_orm::make_column("key", &Settings::key, sqlite_orm::primary_key()),
			sqlite_orm::make_column("value", &Settings::value)));
}




using Storage = decltype(init_storage(""));

class BetDB
{
public:
	Storage storage;
	int buff_count = 100;
	std::vector< BetConfig> buff_write;

	BetDB(const std::string& path) : storage(init_storage(path))
	{
		storage.sync_schema();
	};
	~BetDB() {};

	void set_buffer_size(int cnt) { buff_count = cnt; };

	// -���������� �������� �� ����� � ������� settings
	void set_value(const string& key, const string& value) {
		Settings kv{ key, value };
		storage.insert(kv);
	};

	// -�������� �������� �� ����� � ������� settings
	string get_value(const string& key) {
		if (auto kv = storage.get_pointer<Settings>(key))
			return kv->value;
		return string();
	};

	// - �������� ������ � ��
	void add_bet(BetConfig bet) {
		if (buff_write.size() >= buff_count)
		{
			for (size_t i = 0; i < buff_write.size(); i++)
				storage.insert(buff_write[i]);
			buff_write.clear();
		}
		buff_write.push_back(bet);
	};

	// -�������� ��.
	void	clear() {
		storage.remove_all<BetConfig>();
	};												

	// -�������� ������ �� ������������ ���������� �������
	void	clear(const double start_date, const double stop_date) {
		std::vector<BetConfig> deals;
		if (buff_write.empty())
			deals = storage.get_all<BetConfig>();
		else
			deals = buff_write;

		for (size_t i = 0; i < deals.size(); i++)
			if (deals[i].open_date >= start_date && deals[i].open_date <= stop_date)
				storage.remove<BetConfig>(deals[i].id);
	};

	std::vector<BetConfig> get_bet(
		const double start_date,
		const double stop_date,
		const std::string& signal_name = std::string(),
		const std::string& broker_name = std::string(),
		const std::string& symbol_name = std::string(),
		const uint32_t start_time = 0,
		const uint32_t stop_time = 0) {
		std::vector<BetConfig> deals;
		std::vector<BetConfig> ret;
		if (buff_write.empty())
			deals = storage.get_all<BetConfig>();
		else
			deals = buff_write;

		if (!start_date && !stop_date) { std::cout << "Loh! Pidr!" << std::endl; return ret; }

		//No filters
		if (signal_name.empty() && broker_name.empty() && symbol_name.empty() && start_time == 0 && stop_time == 0) {
			for (size_t i = 0; i < deals.size(); i++)
				if (deals[i].open_date >= start_date &&
					deals[i].open_date <= stop_date) {
					ret.push_back(deals[i]);
				}
			return ret;
		}

		//By Signal, no times
		if (!signal_name.empty() && stop_time == 0) {
			for (size_t i = 0; i < deals.size(); i++)
				if (deals[i].open_date	>= start_date	&&
					deals[i].open_date	<= stop_date	&&
					deals[i].signal		== signal_name) {
					ret.push_back(deals[i]);
				}
		}

		//By Broker, no times
		if (!broker_name.empty() && stop_time == 0) {
			for (size_t i = 0; i < deals.size(); i++)
				if (deals[i].open_date	>= start_date	&&
					deals[i].open_date	<= stop_date	&&
					deals[i].broker		== broker_name) {
					ret.push_back(deals[i]);
				}
		}

		//By Symbol, no times
		if (!symbol_name.empty() && stop_time == 0) {
			for (size_t i = 0; i < deals.size(); i++)
				if (deals[i].open_date	>= start_date	&&
					deals[i].open_date	<= stop_date	&&
					deals[i].symbol		== symbol_name) {
					ret.push_back(deals[i]);
				}
		}

		//-----------------------------------------------
		// time spec
		//+++++++++++++++++++++++++++++++++++++++++++++++

		//By Signal && timespec
		if (!signal_name.empty() && stop_time == 0) {
			for (size_t i = 0; i < deals.size(); i++) {
				uint32_t sec_day = get_second_day(deals[i].open_date);
				if (deals[i].open_date	>= start_date	&&
					deals[i].open_date	<= stop_date	&&
					deals[i].signal		== signal_name	&&
					sec_day				>= start_time	&&
					sec_day				<= stop_time) {

					ret.push_back(deals[i]);
				}
			}
		}

		//By Broker
		if (!broker_name.empty() && stop_time == 0) {
			for (size_t i = 0; i < deals.size(); i++) {
				uint32_t sec_day = get_second_day(deals[i].open_date);
				if (deals[i].open_date	>= start_date	&&
					deals[i].open_date	<= stop_date	&&
					deals[i].broker		== broker_name	&&
					sec_day				>= start_time	&&
					sec_day				<= stop_time
					) {
					ret.push_back(deals[i]);
				}
			}
		}

		//By Symbol
		if (!symbol_name.empty() && stop_time == 0) {
			for (size_t i = 0; i < deals.size(); i++)
			{
				uint32_t sec_day = get_second_day(deals[i].open_date);
				if (deals[i].open_date	>= start_date	&&
					deals[i].open_date	<= stop_date	&&
					deals[i].symbol		== symbol_name	&&
					sec_day				>= start_time	&&
					sec_day				<= stop_time
					) {
					ret.push_back(deals[i]);
				}
			}
		}
		return ret;
	};
};



