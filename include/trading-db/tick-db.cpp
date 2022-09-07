#include "FutureTester.h"
#include "QuotesDB.h"
#include "String.h"



#define TEST
#define DebugDB
TicksBd bd;





void TicksBd::set_quotes(Quotes& q)
{
	q.id = storage.insert(q);
}



//--------------------------------------------------------------------
// Settings table
//
string double_to_str(const float value, int digits = 20)
{
	if (digits > 9 || digits < 0) digits = 9;

	char format[5] = "%.9g";
	format[2] = char(digits + 48);

	char buffer[100];
	sprintf_s(buffer, 100, format, value);

	return buffer;
}


//SYMBOL
void TicksBd::set_symbol( string& broker_name)
{
	Settings s;
	s.key = "SYMBOL";
	s.value = broker_name;
	s.id = storage.insert(s);
}

//DIGITS
void TicksBd::set_digits(double& digits)
{
	Settings s;

	s.key = "DIGITS";
	s.value = double_to_str(digits);
	s.id = storage.insert(s);
}

//SERVER NAME
void TicksBd::set_server_name(string& value)
{
	Settings s;

	s.key = "SERVER_NAME";
	s.value = value;
	s.id = storage.insert(s);
}

//HOST_NAME
void TicksBd::set_hostname(string& name)
{
	Settings s;
	
	s.key = "HOST_NAME";
	s.value = name;
	s.id = storage.insert(s);
}

//COMMENT
void TicksBd::set_comment(string& comment)
{
	Settings s;

	s.key = "COMMENT";
	s.value = comment;
	s.id = storage.insert(s);
}

void TicksBd::set_values(string& key, string& value)
{
	Settings s;

	s.key = key;
	s.value = value;
	s.id = storage.insert(s);
}

std::string TicksBd::get_option(const std::string & key)
{
	if (auto kv = storage.get_pointer<Settings>(key))
		return kv->value;
	else
		return {};
}



void TicksBd::set_periods(Periods& p)
{
	p.id = storage.insert(p);
}

void TicksBd::set_buffer_cap(int capciti)
{
	buffer_capaciti = capciti;
}

void TicksBd::add_price(timestamp_t timestamp, timestamp_t server_timestsmp, double bid, double ask)
{
	if (!recording)
	{
		q_periods++;
		Periods p;
		arr_periods.push_back(p);
		arr_periods[q_periods-1].start_date = timestamp;
		storage.insert(p);
		recording = true;
	}
	if (buffer_capaciti > 60)
	{
		write_db();
	}

	Quotes q;
	q.ask = ask;
	q.bid = bid;
	q.timestamp = timestamp;
	q.server_time = server_timestsmp;
	quotes_buff_w.push_back(q);
}

void TicksBd::stop_recording(timestamp_t timestamp)
{
	if (recording)
	{
		Periods p;
		arr_periods.push_back(p);
		arr_periods[q_periods - 1].stop_date = timestamp;
		storage.insert(p);
		recording = false;
		write_db();
	}
}


TicksBd::TicksBd()
{
	InitDb(30, 166);
	arr_periods= storage.get_all<Periods>();
	q_periods = arr_periods.size();
}


void TicksBd::sort_quotes() {
	if (!std::is_sorted(quotes_buff_w.begin(), quotes_buff_w.end(),
		[](const Quotes & a, const Quotes & b) {
			return a.id < b.id;
		})) {
		std::sort(quotes_buff_w.begin(), quotes_buff_w.end(),
			[](const Quotes & a, const Quotes & b) {
				return a.id < b.id;
			});
	}

}

vector<Quotes>::iterator  TicksBd::search_quote(const Quotes& value)
{
	sort_quotes();

	auto it = std::lower_bound(
		quotes_buff_w.begin(),
		quotes_buff_w.end(),
		value,
		[](const Quotes & l, const Quotes & value) {
			return  l.id < value.id;
		});

	if (it != quotes_buff_w.end())
		return it;
	return quotes_buff_w.end();
}



void TicksBd::InitDb(int min, int max)
{
	storage.sync_schema();


	quotes_buff_r = storage.get_all<Quotes>(where(
		c(&Quotes::timestamp) <= max &&
		c(&Quotes::timestamp) >= min));
}

pair<double, double> TicksBd::get_one_price(timestamp_t timestamp)
{
	auto ask = storage.select(&Quotes::ask, where(c(&Quotes::timestamp) == timestamp));
	auto bid = storage.select(&Quotes::bid, where(c(&Quotes::timestamp) == timestamp));
	if (ask.size())
		return make_pair(ask[0], bid[0]);
	return make_pair(NULL, NULL);
}

bool TicksBd::write_db()
{
	storage.transaction([&] {
		for (Quotes& quote : quotes_buff_w) {
			storage.insert(quote);
		}
		return true;
		});
	quotes_buff_w.clear();
	return false;
}



pair<double, double> TicksBd::get_price(timestamp_t timestamp)
{

	auto it = std::lower_bound(
		arr_periods.begin(),
		arr_periods.end(),
		timestamp,
		[](const Periods & l, timestamp_t timestamp) {
			return  l.start_date < timestamp;
		});
	if (it == arr_periods.end()) return make_pair(0, 0);


	it = std::lower_bound(
		arr_periods.begin(),
		arr_periods.end(),
		timestamp,
		[](const Periods & l, timestamp_t timestamp) {
			return  l.stop_date < timestamp;
		});
	if (it == arr_periods.end()) return make_pair(0, 0);


	pair<double, double> price;
	price = get_buff_price(timestamp);


	if (price.first > 0)
	{
		quotes_buff_r = storage.get_all<Quotes>(where(
			c(&Quotes::timestamp) <= max &&
			c(&Quotes::timestamp) >= min));

	}
	price = get_buff_price(timestamp);

	return price;
}

pair<double, double> TicksBd::get_buff_price(timestamp_t timestamp)
{
	auto it_r = std::lower_bound(
		quotes_buff_r.begin(),
		quotes_buff_r.end(),
		timestamp,
		[](const Quotes & l, timestamp_t timestamp) {
			return  l.timestamp < timestamp;
		});

	if (it_r  == quotes_buff_r.end())
		return make_pair(it_r->ask, it_r->bid);



	auto it_w = std::lower_bound(
		quotes_buff_w.begin(),
		quotes_buff_w.end(),
		timestamp,
		[](const Quotes & l, timestamp_t timestamp) {
			return  l.timestamp < timestamp;
		});

	if (it_w == quotes_buff_w.end())
		return make_pair(it_w->ask, it_w->bid);
}





/*
TRASH

//pair<vector<Quotes>, vector<double, allocator<double>>> ts_quotes = make_pair(storage.get_all<Quotes>(), storage.select(&Quotes::timestamp));



for (size_t i = 0; i < 200; i++)
	{
		add_price(i + 1, i + 1, 115 + i, 116 + i);

	}
	quotes_buff_w = quotes_buff_w;

	storage.transaction([&] {
		for (Quotes& quote : quotes_buff_w) {
			storage.insert(quote);
		}
		return true;
		});


	//get timestamps array fromdb
	auto allLastNames = storage.select(&Quotes::timestamp, where(
		c(&Quotes::timestamp) <= backward &&
		c(&Quotes::timestamp) >= forward));
	cout << "allLastNames count = " << allLastNames.size() << endl; //  allLastNames is std::vector<std::string>
	for (auto& lastName : allLastNames) {
		cout << " " << lastName << endl;
	}

	return make_pair(NULL, NULL);
*/