#pragma once

using namespace std;
using namespace _xtime;





struct Quotes
{
	int id = -1;
	double timestamp = 3355;
	double server_time = 23111;
	double bid = 0;
	double ask = 0;
};

struct Periods
{
	int id = -1;
	double start_date;
	double stop_date;
};

struct Settings
{
	int id = -1;
	string key;
	string value;
};



using namespace sqlite_orm;



inline auto initStorage(const string& path) {
	{
		return make_storage(path,
			make_table("Quotes",
				make_column("id", &Quotes::id, unique(), primary_key()),
				make_column("timestamp", &Quotes::timestamp),
				make_column("server_time", &Quotes::server_time),
				make_column("bid", &Quotes::bid),
				make_column("ask", &Quotes::ask)),
			make_table("Periods",
				make_column("id", &Periods::id, unique(), primary_key()),
				make_column("start_date", &Periods::start_date),
				make_column("stop_date", &Periods::stop_date)),
			make_table("Settings",
				make_column("id", &Settings::id, unique(), primary_key()),
				make_column("key", &Settings::key),
				make_column("value", &Settings::value)));
	};
}
using Storage = decltype(initStorage(""));


class TicksBd
{
	Storage storage = initStorage("storage_.sqlite");
	int buffer_capaciti = 60;
	int precesion = 1;
	int min,max = 5;
	int q_periods = 0;
	bool recording = false;
	vector<Quotes> quotes_buff_r;
	vector<Quotes> quotes_buff_w;
	vector<Periods> arr_periods;
public:
	TicksBd();
	void sort_quotes();
	vector<Quotes>::iterator  search_quote(const Quotes& value);
	void InitDb(int min, int max);
	void set_quotes(Quotes& q);



	//Settings table
	void set_symbol(string& brok_name);
	void set_digits(double& digits);
	void set_server_name(string&);
	void set_hostname(string& name);
	void set_comment(string& comment);
	void set_values(string& key, string& value);


	//Periods table
	void set_periods(Periods& p);
	std::string get_option(const std::string& key);


	void set_region(int min, int max) { min = min; max = max; };

	void set_buffer_cap(int capciti);

	void add_price(timestamp_t timestamp, timestamp_t server_timestsmp, double bid, double ask);
	
	void stop_recording(timestamp_t timestamp);

private:
	pair<double, double> get_price(timestamp_t timestamp);
	pair<double, double> get_buff_price(timestamp_t timestamp);
	pair<double, double> get_one_price(timestamp_t timestamp);
	bool write_db();

};