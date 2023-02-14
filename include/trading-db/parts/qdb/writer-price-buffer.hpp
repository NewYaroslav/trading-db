#pragma once
#ifndef TRADING_DB_QDB_WRITER_PRICE_BUFFER_HPP_INCLUDED
#define TRADING_DB_QDB_WRITER_PRICE_BUFFER_HPP_INCLUDED

#include "enums.hpp"
#include "data-classes.hpp"
#include "ztime.hpp"
#include <functional>
#include <map>
#include <array>
#include <vector>

namespace trading_db {

	/** \brief Буфер для хранения данных цен тиков и баров
	 */
	class QdbWriterPriceBuffer {
	public:

		QdbWriterPriceBuffer() {};

		~QdbWriterPriceBuffer() {};

		std::function<void(
			const std::map<uint64_t, ShortTick> &ticks,
			const uint64_t t)>	on_ticks	= nullptr;

		std::function<void(
			const std::array<Candle, ztime::MIN_PER_DAY> &candles,
			const uint64_t t)>	on_candles	= nullptr;

	private:
		std::map<uint64_t, ShortTick>				ticks_buffer;
		std::array<Candle, ztime::MIN_PER_DAY>	    candles_buffer;
		uint64_t time_ticks_buffer = 0;
		uint64_t time_candles_buffer = 0;

		inline void erase_candles_buffer() noexcept {
			for (auto &c : candles_buffer) {
				c.open = c.close = c.high = c.low = c.volume = c.timestamp = 0;
			}
		}

	public:

		inline void start() noexcept {
			time_ticks_buffer = 0;
			time_candles_buffer = 0;
			ticks_buffer.clear();
			erase_candles_buffer();
		}

		inline void stop() noexcept {
			if (!ticks_buffer.empty() && time_ticks_buffer) {
				if (on_ticks) on_ticks(ticks_buffer, time_ticks_buffer);
				ticks_buffer.clear();
			}
			if (time_candles_buffer) {
				if (on_candles) on_candles(candles_buffer, time_candles_buffer);
				erase_candles_buffer();
			}
		}

		void write(const Candle &candle) noexcept {
			const uint64_t timestamp_day = ztime::start_of_day(candle.timestamp);
			if (timestamp_day != time_candles_buffer) {
				if (time_candles_buffer) {
					if (on_candles) on_candles(candles_buffer, time_candles_buffer);
					erase_candles_buffer();
				}
				time_candles_buffer = timestamp_day;
			}
			const size_t minute_day = ztime::get_minute_day(candle.timestamp);
			candles_buffer[minute_day] = candle;
		}

		void write(const Tick &tick) {
			const uint64_t timestamp_hour = ztime::start_of_hour_sec(tick.timestamp_ms);
			if (timestamp_hour != time_ticks_buffer) {
				if (time_ticks_buffer) {
					if (on_ticks) on_ticks(ticks_buffer, time_ticks_buffer);
					ticks_buffer.clear();
				}
				time_ticks_buffer = timestamp_hour;
			}
			ticks_buffer[tick.timestamp_ms] = ShortTick(tick.bid, tick.ask);
		}

	};
}

#endif // TRADING_DB_QDB_WRITER_PRICE_BUFFER_HPP_INCLUDED
