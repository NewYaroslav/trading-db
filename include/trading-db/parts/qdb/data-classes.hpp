#pragma once
#ifndef TRADING_DB_QDB_DATA_CLASSES_HPP_INCLUDED
#define TRADING_DB_QDB_DATA_CLASSES_HPP_INCLUDED

#include <limits>
#include "ztime.hpp"

namespace trading_db {

	/** \brief Класс для хранения бара
	 */
	class Candle {
	public:
		double		open;
		double		high;
		double		low;
		double		close;
		double		volume;
		uint64_t	timestamp;

		Candle() :
			open(0),
			high(0),
			low (0),
			close(0),
			volume(0),
			timestamp(0) {
		};

		Candle(
				const double &new_open,
				const double &new_high,
				const double &new_low,
				const double &new_close,
				const uint64_t &new_timestamp) :
			open(new_open),
			high(new_high),
			low (new_low),
			close(new_close),
			volume(0),
			timestamp(new_timestamp) {}

		Candle(
				const double &new_open,
				const double &new_high,
				const double &new_low,
				const double &new_close,
				const double &new_volume,
				const uint64_t &new_timestamp) :
			open(new_open),
			high(new_high),
			low (new_low),
			close(new_close),
			volume(new_volume),
			timestamp(new_timestamp) {}

		bool empty() const noexcept {
			return (timestamp == 0 || close == 0);
		}
	}; // Candle

	/** \brief Класс для хранения урезанных данных бара (без времени)
	 */
	class ShortCandle {
	public:
		double		open;
		double		high;
		double		low;
		double		close;
		double		volume;

		ShortCandle() :
			open(0),
			high(0),
			low (0),
			close(0),
			volume(0) {
		};

		ShortCandle(
				const double &new_open,
				const double &new_high,
				const double &new_low,
				const double &new_close) :
			open(new_open),
			high(new_high),
			low (new_low),
			close(new_close),
			volume(0) {}

		ShortCandle(
				const double &new_open,
				const double &new_high,
				const double &new_low,
				const double &new_close,
				const double &new_volume) :
			open(new_open),
			high(new_high),
			low (new_low),
			close(new_close),
			volume(new_volume) {}

		bool empty() const noexcept {
			return (close == 0);
		}
	}; // ShortCandle

	/** \brief Класс для хранения тика
	 */
	class Tick {
	public:
		double		bid;
		double		ask;
		uint64_t	timestamp_ms;

		Tick() :
			bid(0),
			ask(0),
			timestamp_ms(0) {
		};

		Tick(	const double &new_bid,
				const double &new_ask,
				const uint64_t &new_timestamp_ms) :
			bid(new_bid),
			ask(new_ask),
			timestamp_ms(new_timestamp_ms) {}

		bool empty() const noexcept {
			return (timestamp_ms == 0);
		}
	}; // Tick

	/** \brief Класс для хранения урезанных данных тика (без времени)
	 */
	class ShortTick {
	public:
		double		bid;
		double		ask;

		ShortTick() :
			bid(0),
			ask(0) {
		};

		ShortTick(
				const double &new_bid,
				const double &new_ask) :
			bid(new_bid),
			ask(new_ask) {
		}

		bool empty() const noexcept {
			return (bid == 0);
		}
	}; // ShortTick

	/** \brief Класс точки времени
	 */
	class TimePoint {
	public:
		uint32_t second_day = 0;

		inline void set(
				const int hh,
				const int mm,
				const int ss = 0) noexcept {
			second_day = ztime::get_second_day(hh, mm, ss);
		}

		TimePoint() {};
		TimePoint(const int hh, const int mm, const int ss = 0) { set(hh, mm, ss); };
	}; // TimePoint

	static const int32_t QDB_TIME_PERIOD_NO_ID = std::numeric_limits<int32_t>::min();	/**< Значение для пустого ID периода */

	/** \brief Класс периода времени
	 * \warning Период включает в себя время, указанное в stop
	 */
	class TimePeriod {
	public:
		TimePoint	start;
		TimePoint	stop;
		int32_t		id		= QDB_TIME_PERIOD_NO_ID;

		inline const bool check_time(const uint64_t t) const noexcept {
			const uint32_t second_day = ztime::get_second_day(t);
			return (second_day >= start.second_day && second_day <= stop.second_day);
		}

		inline void set(
				const TimePoint &user_start,
				const TimePoint &user_stop,
				const int32_t user_id = QDB_TIME_PERIOD_NO_ID) noexcept {
			start = user_start;
			stop = user_stop;
			id = user_id;
		}

		TimePeriod() {};

		TimePeriod(
				const TimePoint &user_start,
				const TimePoint &user_stop,
				const int32_t user_id = QDB_TIME_PERIOD_NO_ID) {
				set(user_start, user_stop, user_id);
		};
	}; // TimePeriod

};

#endif // TRADING_DB_QDB_DATA_CLASSES_HPP_INCLUDED
