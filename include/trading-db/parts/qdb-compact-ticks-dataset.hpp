#pragma once
#ifndef TRADING_DB_QDB_COMPACT_TICKS_DATASET_HPP_INCLUDED
#define TRADING_DB_QDB_COMPACT_TICKS_DATASET_HPP_INCLUDED

#include "qdb-common.hpp"
#include <vector>
#include <cmath>
#include "ztime.hpp"

namespace trading_db {

	class QdbCompactTickDataset {
	private:
		// reg_a - множитель цены и объема (8 бит)
		//
		// 0-3 биты - множитель цены (4 бит), значения: 1,10,100,...
		// 4-7 биты - множитель объема (4 бит), значения: 1,10,100,...
		//
		// reg_b - регистр разрядности переменных для хранения цены и объема (8 бит)
		//
		// 0-1 биты - кол-во бит для начальной цены (2 бита)
		// 2-3 биты - кол-во бит для дельты цены (2 бита)
		// 4-5 биты - кол-во бит для начального объема (2 бита)
		// 6-7 биты - кол-во бит для дельты объема (2 бита)
		//
		// reg_c - unix день (16 бит) (опционально)
		//
		// header - содержит две переменные с начальной ценой и объемом
		//
		//
		// data - данные баров
		// - длина зависит от настроек
		// - всегда содержит 1440 сэмпла (баров)
		// - формат одного сэмпа (бара): OHLCV
		// - сэмп хранит дельты
		// - крайние отрицательные значения обозначают отсутствие бара
		std::vector<uint8_t> data;

		// рассчитать размер беззнакового целочисленного типа
		inline uint8_t calc_uint_type(const uint64_t v) noexcept {
			if (v >> 32) return 0x03;
			if (v >> 16) return 0x02;
			if (v >> 8) return 0x01;
			return 0x00;
		}

		// рассчитать размер знакового целочисленного типа
		inline uint8_t calc_int_type(const uint64_t v) noexcept {
			if (v <= 127) return 0x00;
			if (v <= 32767) return 0x01;
			if (v <= 2147483647) return 0x02;
			return 0x03;
		}

		inline size_t conv_int_type_to_bytes(const uint8_t t) noexcept {
			switch (t) {
			case 0:
				return sizeof(uint8_t);
			case 1:
				return sizeof(uint16_t);
			case 2:
				return sizeof(uint32_t);
			case 3:
				return sizeof(uint64_t);
			};
			return sizeof(uint8_t);
		}

		template<class T1, class T2>
		inline void set_ptr(const T2 value, const uint8_t* p, const size_t offset_ptr, const size_t offset) {
			// uint8_t* p = data.data();
			T1 *ptr = (T1*)(&p[offset_ptr]);
			ptr[offset] = static_cast<T1>(value);
		}

		template<class T1, class T2>
		inline void get_ptr(T2 &value, const uint8_t* p, const size_t offset_ptr, const size_t offset) {
			// uint8_t* p = data.data();
			const T1 *ptr = (const T1*)(&p[offset_ptr]);
			value = static_cast<T2>(ptr[offset]);
		}

		inline size_t set_u64_value(const uint64_t value, const uint8_t type, const uint8_t* p, size_t offset_ptr, const size_t offset = 0) {
			switch (type) {
			case 0: // uint8_t
				set_ptr<uint8_t, uint64_t>(value, p, offset_ptr, offset);
				offset_ptr += sizeof(uint8_t);
				break;
			case 1: // uint16_t
				set_ptr<uint16_t, uint64_t>(value, p, offset_ptr, offset);
				offset_ptr += sizeof(uint16_t);
				break;
			case 2: // uint32_t
				set_ptr<uint32_t, uint64_t>(value, p, offset_ptr, offset);
				offset_ptr += sizeof(uint32_t);
				break;
			case 3: // uint64_t
				set_ptr<uint64_t, uint64_t>(value, p, offset_ptr, offset);
				offset_ptr += sizeof(uint64_t);
				break;
			};
			return offset_ptr;
		}

		inline size_t get_u64_value(uint64_t &value, const uint8_t type, const uint8_t* p, size_t offset_ptr, const size_t offset = 0) {
			switch (type) {
			case 0: // uint8_t
				get_ptr<uint8_t, uint64_t>(value, p, offset_ptr, offset);
				offset_ptr += sizeof(uint8_t);
				break;
			case 1: // uint16_t
				get_ptr<uint16_t, uint64_t>(value, p, offset_ptr, offset);
				offset_ptr += sizeof(uint16_t);
				break;
			case 2: // uint32_t
				get_ptr<uint32_t, uint64_t>(value, p, offset_ptr, offset);
				offset_ptr += sizeof(uint32_t);
				break;
			case 3: // uint64_t
				get_ptr<uint64_t, uint64_t>(value, p, offset_ptr, offset);
				offset_ptr += sizeof(uint64_t);
				break;
			};
			return offset_ptr;
		}

		inline size_t set_s64_value(const int64_t value, const uint8_t type, const uint8_t* p, size_t offset_ptr, const size_t offset = 0) {
			switch (type) {
			case 0: // int8_t
				set_ptr<int8_t, int64_t>(value, p, offset_ptr, offset);
				offset_ptr += sizeof(int8_t);
				break;
			case 1: // int16_t
				set_ptr<int16_t, int64_t>(value, p, offset_ptr, offset);
				offset_ptr += sizeof(int16_t);
				break;
			case 2: // int32_t
				set_ptr<int32_t, int64_t>(value, p, offset_ptr, offset);
				offset_ptr += sizeof(int32_t);
				break;
			case 3: // int64_t
				set_ptr<int64_t, int64_t>(value, p, offset_ptr, offset);
				offset_ptr += sizeof(int64_t);
				break;
			};
			return offset_ptr;
		}

		inline size_t get_s64_value(int64_t &value, const uint8_t type, const uint8_t* p, size_t offset_ptr, const size_t offset = 0) {
			switch (type) {
			case 0: // int8_t
				get_ptr<int8_t, int64_t>(value, p, offset_ptr, offset);
				offset_ptr += sizeof(int8_t);
				break;
			case 1: // int16_t
				get_ptr<int16_t, int64_t>(value, p, offset_ptr, offset);
				offset_ptr += sizeof(int16_t);
				break;
			case 2: // int32_t
				get_ptr<int32_t, int64_t>(value, p, offset_ptr, offset);
				offset_ptr += sizeof(int32_t);
				break;
			case 3: // int64_t
				get_ptr<int64_t, int64_t>(value, p, offset_ptr, offset);
				offset_ptr += sizeof(int64_t);
				break;
			};
			return offset_ptr;
		}

		inline size_t set_no_value(const uint8_t type, const uint8_t* p, size_t offset_ptr, const size_t offset = 0) {
			switch (type) {
			case 0: // uint8_t
				set_ptr<uint8_t, uint8_t>(0x80, p, offset_ptr, offset);
				offset_ptr += sizeof(uint8_t);
				break;
			case 1: // uint16_t
				set_ptr<uint16_t, uint16_t>(0x8000, p, offset_ptr, offset);
				offset_ptr += sizeof(uint16_t);
				break;
			case 2: // uint32_t
				set_ptr<uint32_t, uint32_t>(0x80000000, p, offset_ptr, offset);
				offset_ptr += sizeof(uint32_t);
				break;
			case 3: // uint64_t
				set_ptr<uint64_t, uint64_t>(0x8000000000000000, p, offset_ptr, offset);
				offset_ptr += sizeof(uint64_t);
				break;
			};
			return offset_ptr;
		}

		inline size_t check_no_value(bool &status, const uint8_t type, const uint8_t* p, size_t offset_ptr, const size_t offset = 0) {
			status = false;
			switch (type) {
			case 0: { // uint8_t
					uint8_t value = 0;
					get_ptr<uint8_t, uint8_t>(value, p, offset_ptr, offset);
					offset_ptr += sizeof(uint8_t);
					if (value != 0x80) status = true;
					//std::cout << "value " << value << std::endl;
				}
				break;
			case 1: { // uint16_t
					uint16_t value = 0;
					get_ptr<uint16_t, uint16_t>(value, p, offset_ptr, offset);
					offset_ptr += sizeof(uint16_t);
					if (value != 0x8000) status = true;
					//std::cout << "value " << value << std::endl;
				}
				break;
			case 2: { // uint32_t
					uint32_t value = 0;
					get_ptr<uint32_t, uint32_t>(value, p, offset_ptr, offset);
					offset_ptr += sizeof(uint32_t);
					if (value != 0x80000000) status = true;
					//std::cout << "value " << value << std::endl;
				}
				break;
			case 3: { // uint64_t
					uint64_t value = 0;
					get_ptr<uint64_t, uint64_t>(value, p, offset_ptr, offset);
					offset_ptr += sizeof(uint64_t);
					if (value != 0x8000000000000000) status = true;
					//std::cout << "value " << value << std::endl;
				}
				break;
			};
			return offset_ptr;
		}

	public:

		QdbCompactTickDataset() {};

		inline std::vector<uint8_t> &get_data() noexcept {
			return data;
		}

		/** \brief Записать последовательность тиков
		 * \param ticks			Последовательность цен
		 * \param price_scale	Множитель для цены (количество знаков послезапятой)
		 */
		template<class T>
		inline void write_sequence(
				const T &ticks,
				const size_t price_scale,
				const uint64_t timestamp_ms) noexcept {
			if (ticks.empty()) return;

			auto it_begin = ticks.begin();

			double max_diff_price = 0;
			int64_t max_diff_time = 0;
			double last_price = it_begin->bid;
			//int64_t last_time = it_begin->timestamp_ms;
			int64_t last_time = timestamp_ms;
			for (const auto &tick : ticks) {
				// обновляем значение максимальной разницы цены
				max_diff_price = std::max(max_diff_price, std::max(std::abs(tick.bid - last_price), std::abs(tick.ask - last_price)));
				last_price = tick.bid;

				// обновляем значение максимальной разницы времени
				const int64_t diff_time = std::abs((int64_t)tick.timestamp_ms - last_time);
				max_diff_time = std::max(max_diff_time, diff_time);
				last_time = tick.timestamp_ms;
			}

			// находим множитель цены
			const uint64_t price_factor = (uint64_t)(std::pow(10, price_scale) + 0.5d);

			// находим начальную амплитуду и начальное время
			const uint64_t start_amplitude_price = (uint64_t)((it_begin->bid * (double)price_factor) + 0.5d);
			//const uint64_t start_time = it_begin->timestamp_ms;
			const uint64_t start_time = timestamp_ms;

			// максимальная разница цены
			const uint64_t max_diff_amplitude_price = (uint64_t)((max_diff_price * (double)price_factor) + 0.5d);

			const uint8_t reg_a0 = (uint8_t)(price_scale & 0x0F);
			const uint8_t reg_a = reg_a0;

			// определяем тип переменной
			const uint8_t reg_b0 = calc_uint_type(start_amplitude_price);
			const uint8_t reg_b1 = calc_int_type(max_diff_amplitude_price);
			//const uint8_t reg_b2 = calc_uint_type(start_time);
			const uint8_t reg_b3 = calc_int_type(max_diff_time);
			//const uint8_t reg_b = (reg_b3 << 6) | (reg_b2 << 4) | (reg_b1 << 2) | (reg_b0 & 0x03);
			const uint8_t reg_b = (reg_b3 << 6) | (reg_b1 << 2) | (reg_b0 & 0x03);

			// определяем длину массива в зависимости от наличия метки времени
			const size_t length = 2 +
				//conv_int_type_to_bytes(reg_b0) + conv_int_type_to_bytes(reg_b2) +
				conv_int_type_to_bytes(reg_b0) +
				2 * conv_int_type_to_bytes(reg_b1) * ticks.size() +
				conv_int_type_to_bytes(reg_b3) * ticks.size();

			data.resize(length);

			// записываем регистры
			data[0] = reg_a;
			data[1] = reg_b;

			uint8_t* p = data.data();
			size_t offset_ptr = 2;

			// записываем начальную цену
			offset_ptr = set_u64_value(start_amplitude_price, reg_b0, p, offset_ptr);
			// записываем начальное время
			//offset_ptr = set_u64_value(start_time, reg_b2, p, offset_ptr);
			const size_t sample_size = 2 * conv_int_type_to_bytes(reg_b1) + conv_int_type_to_bytes(reg_b3);

			// инициализируем отсутствие данных для всех баров
			for (size_t i = 0; i < ticks.size(); ++i) {
				size_t sample_offset_ptr = offset_ptr + i * sample_size;
				sample_offset_ptr = set_no_value(reg_b1, p, sample_offset_ptr);
				sample_offset_ptr = set_no_value(reg_b1, p, sample_offset_ptr);
									set_no_value(reg_b3, p, sample_offset_ptr);
			}

			// записываем те бары, что есть в наличии
			uint64_t last_p = start_amplitude_price;
			uint64_t last_t = start_time;
			size_t tick_counter = 0;
			for (auto &tick : ticks) {
				const int64_t bid = (int64_t)((tick.bid * (double)price_factor) + 0.5d);
				const int64_t ask = (int64_t)((tick.ask * (double)price_factor) + 0.5d);
				const int64_t t = (int64_t)(tick.timestamp_ms);

				const int64_t db = bid - (int64_t)last_p;
				const int64_t da = ask - (int64_t)last_p;
				const int64_t dt = t - (int64_t)last_t;

				size_t sample_offset_ptr = offset_ptr + sample_size * tick_counter;
				++tick_counter;
				sample_offset_ptr = set_s64_value(db, reg_b1, p, sample_offset_ptr);
				sample_offset_ptr = set_s64_value(da, reg_b1, p, sample_offset_ptr);
									set_s64_value(dt, reg_b3, p, sample_offset_ptr);
				last_p = bid;
				last_t = t;
			}
		} // write_sequence

		template<class T>
		inline void read_sequence(
				T				&ticks,
				size_t			&price_scale,
				const uint64_t	timestamp_ms) {

			if (data.empty()) {
				price_scale = 0;
				return;
			}

			// получаем точность котировок и объема
			price_scale = data[0] & 0x0F;			// reg_a 0-3

			// получаем множитель для котировок иобъема
			const uint64_t price_factor = (uint64_t)(std::pow(10, price_scale) + 0.5d);

			// получаем количество байт для хранения данных
			const uint8_t reg_b0 = data[1] & 0x03;
			const uint8_t reg_b1 = (data[1] >> 2) & 0x03;
			//const uint8_t reg_b2 = (data[1] >> 4) & 0x03;
			const uint8_t reg_b3 = (data[1] >> 6) & 0x03;

			const uint8_t* p = data.data();
			size_t offset_ptr = 2;

			const size_t sample_size = 2 * conv_int_type_to_bytes(reg_b1) + conv_int_type_to_bytes(reg_b3);

			// получаем стартовую цену и время
			uint64_t start_price = 0;//, start_time = 0;
			offset_ptr = get_u64_value(start_price, reg_b0, p, offset_ptr);
			//offset_ptr = get_u64_value(start_time, reg_b2, p, offset_ptr);
			//uint64_t last_price = start_price, last_time = start_time;
			uint64_t last_price = start_price, last_time = timestamp_ms;

			size_t index = 0;
			while (!false) {
				size_t sample_offset_ptr = offset_ptr + index * sample_size;
				if (sample_offset_ptr >= data.size()) break;
				++index;

				int64_t db = 0, da = 0, dt = 0;
				sample_offset_ptr = get_s64_value(db, reg_b1, p, sample_offset_ptr);
				sample_offset_ptr = get_s64_value(da, reg_b1, p, sample_offset_ptr);
									get_s64_value(dt, reg_b3, p, sample_offset_ptr);
				const int64_t b = last_price + db;
				const int64_t a = last_price + da;
				const int64_t t = last_time + dt;
				last_price = b;
				last_time = t;

				const double bid = (double)b / (double)price_factor;
				const double ask = (double)a / (double)price_factor;
				ticks.emplace_back(bid, ask, t);
			};
		} // read_sequence
	};

};

#endif // TRADING_DB_QDB_COMPACT_TICKS_DATASET_HPP_INCLUDED
