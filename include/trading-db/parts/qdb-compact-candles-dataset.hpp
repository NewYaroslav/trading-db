#pragma once
#ifndef TRADING_DB_QDB_COMPACT_CANDLES_DATASET_HPP_INCLUDED
#define TRADING_DB_QDB_COMPACT_CANDLES_DATASET_HPP_INCLUDED

#include "qdb-common.hpp"
#include <vector>
#include <cmath>
#include "ztime.hpp"

namespace trading_db {

    /** \brief Класс для компактного хранения массива баров
     */
    class QdbCompactCandlesDataset {
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

        QdbCompactCandlesDataset() {};

        inline std::vector<uint8_t> &get_data() noexcept {
            return data;
        }

        /** \brief Записать последовательность цен
         *
         * \warning Последовательность цен должна содержать 1440 минутных баров
         * \param candles       Последовательность цен
         * \param price_scale   Множитель для цены (количество знаков послезапятой)
         * \param volume_scale  Множитель для объема (количество знаков послезапятой)
         */
        template<class T>
        inline void write_sequence(
                const T &candles,
                const size_t price_scale,
                const size_t volume_scale) noexcept {
            if (candles.empty()) return;
            auto it_begin = candles.begin();

            const double sh = it_begin->high;
            const double sl = it_begin->low;
            const double sc = it_begin->close;
            const double sv = it_begin->volume;
            const uint64_t st = it_begin->timestamp;

            double max_diff_price = std::max(std::abs(sh - sc), std::abs(sl - sc));
            double max_diff_volume = 0;
            double last_price = sc, last_volume = sv;
            for (const auto &c : candles) {
                // обновляем значение максимальной разницы цены
                max_diff_price = std::max(max_diff_price, std::max(std::abs(c.high - last_price), std::abs(c.low - last_price)));
                last_price = c.close;

                // обновляем значение максимальной разницы объема
                const double diff_volume = std::abs(c.volume - last_volume);
                max_diff_volume = std::max(max_diff_volume, diff_volume);
                last_volume = c.volume;
            }

            // находим множитель цены и объема
            const uint64_t price_factor = (uint64_t)(std::pow(10, price_scale) + 0.5d);
            const uint64_t volume_factor = (uint64_t)(std::pow(10, volume_scale) + 0.5d);

            // находим начальную амплитуду
            const uint64_t start_amplitude_price = (uint64_t)((sc * (double)price_factor) + 0.5d);
            const uint64_t start_amplitude_volume = (uint64_t)((sv * (double)volume_factor) + 0.5d);

            // максимальная разница цены и объема
            const uint64_t max_diff_amplitude_price = (uint64_t)((max_diff_price * (double)price_factor) + 0.5d);
            const uint64_t max_diff_amplitude_volume = (uint64_t)((max_diff_volume * (double)volume_factor) + 0.5d);

            const uint8_t reg_a0 = (uint8_t)(price_scale & 0x0F);
            const uint8_t reg_a1 = (uint8_t)(volume_scale & 0x0F);
            const uint8_t reg_a = reg_a0 | (reg_a1 << 4);

            // определяем тип переменной
            const uint8_t reg_b0 = calc_uint_type(start_amplitude_price);
            const uint8_t reg_b1 = calc_int_type(max_diff_amplitude_price);
            const uint8_t reg_b2 = calc_uint_type(start_amplitude_volume);
            const uint8_t reg_b3 = calc_int_type(max_diff_amplitude_volume);
            const uint8_t reg_b = (reg_b3 << 6) | (reg_b2 << 4) | (reg_b1 << 2) | (reg_b0 & 0x03);

            // определяем длину массива в зависимости от наличия метки времени
            const size_t length = 2 +
                conv_int_type_to_bytes(reg_b0) + conv_int_type_to_bytes(reg_b2) +
                4 * conv_int_type_to_bytes(reg_b1) * ztime::MINUTES_IN_DAY +
                conv_int_type_to_bytes(reg_b3) * ztime::MINUTES_IN_DAY;

            data.resize(length);

            // записываем регистры
            data[0] = reg_a;
            data[1] = reg_b;
            /*
            if (timestamp_day == 0) {
                // находим unix день
                const uint16_t reg_c = ztime::get_day(st);
                data[2] = (uint8_t)reg_c;
                data[3] = (uint8_t)(reg_c >> 8);
            }
            */

            uint8_t* p = data.data();
            size_t offset_ptr = 2;

            // записываем начальную цену
            offset_ptr = set_u64_value(start_amplitude_price, reg_b0, p, offset_ptr);
            // записываем начальный объем
            offset_ptr = set_u64_value(start_amplitude_volume, reg_b2, p, offset_ptr);
            const size_t sample_size = 4 * conv_int_type_to_bytes(reg_b1) + conv_int_type_to_bytes(reg_b3);

            // инициализируем отсутствие данных для всех баров
            for (size_t i = 0; i < ztime::MINUTES_IN_DAY; ++i) {
                size_t sample_offset_ptr = offset_ptr + i * sample_size;
                sample_offset_ptr = set_no_value(reg_b1, p, sample_offset_ptr);
                sample_offset_ptr = set_no_value(reg_b1, p, sample_offset_ptr);
                sample_offset_ptr = set_no_value(reg_b1, p, sample_offset_ptr);
                sample_offset_ptr = set_no_value(reg_b1, p, sample_offset_ptr);
                                    set_no_value(reg_b3, p, sample_offset_ptr);
            }

            // записываем те бары, что есть в наличии
            uint64_t last_cc = start_amplitude_price;
            uint64_t last_cv = start_amplitude_volume;
            for (auto &c : candles) {
                const int64_t co = (int64_t)((c.open * (double)price_factor) + 0.5d);
                const int64_t ch = (int64_t)((c.high * (double)price_factor) + 0.5d);
                const int64_t cl = (int64_t)((c.low * (double)price_factor) + 0.5d);
                const int64_t cc = (int64_t)((c.close * (double)price_factor) + 0.5d);
                const int64_t cv = (int64_t)((c.volume * (double)volume_factor) + 0.5d);

                const int64_t cdo = co - (int64_t)last_cc;
                const int64_t cdh = ch - (int64_t)last_cc;
                const int64_t cdl = cl - (int64_t)last_cc;
                const int64_t cdc = cc - (int64_t)last_cc;
                const int64_t cdv = cv - (int64_t)last_cv;

                size_t sample_offset_ptr = offset_ptr + sample_size * ztime::get_minute_day(c.timestamp);
                sample_offset_ptr = set_s64_value(cdo, reg_b1, p, sample_offset_ptr);
                sample_offset_ptr = set_s64_value(cdh, reg_b1, p, sample_offset_ptr);
                sample_offset_ptr = set_s64_value(cdl, reg_b1, p, sample_offset_ptr);
                sample_offset_ptr = set_s64_value(cdc, reg_b1, p, sample_offset_ptr);
                                    set_s64_value(cdv, reg_b3, p, sample_offset_ptr);
                last_cc = cc;
                last_cv = cv;
            }
        } // write_sequence

        template<class T>
        inline void read_sequence(
                T &candles,
                size_t &price_scale,
                size_t &volume_scale,
                const uint64_t timestamp_day = 0,
                const bool is_fill = false) {
            if (data.empty()) {
                price_scale = 0;
                volume_scale = 0;
                return;
            }

            // получаем точность котировок и объема
            price_scale = data[0] & 0x0F;			// reg_a 0-3
            volume_scale = (data[0] >> 4) & 0x0F;	// reg_a 4-7

            // получаем множитель для котировок иобъема
            const uint64_t price_factor = (uint64_t)(std::pow(10, price_scale) + 0.5d);
            const uint64_t volume_factor = (uint64_t)(std::pow(10, volume_scale) + 0.5d);

            // получаем количество байт для хранения данных
            const uint8_t reg_b0 = data[1] & 0x03;
            const uint8_t reg_b1 = (data[1] >> 2) & 0x03;
            const uint8_t reg_b2 = (data[1] >> 4) & 0x03;
            const uint8_t reg_b3 = (data[1] >> 6) & 0x03;

            // получаем метку времени
            //const uint64_t start_timestamp = timestamp_day == 0 ? ztime::SECONDS_IN_DAY * (uint64_t)((((uint16_t)data[3]) << 8) | data[2]) : timestamp_day;
            const uint64_t start_timestamp = timestamp_day;

            const uint8_t* p = data.data();
            size_t offset_ptr = 2;

            const size_t sample_size = 4 * conv_int_type_to_bytes(reg_b1) + conv_int_type_to_bytes(reg_b3);

            // получаем стартовую цену и объем
            uint64_t start_price = 0, start_volume = 0;
            offset_ptr = get_u64_value(start_price, reg_b0, p, offset_ptr);
            offset_ptr = get_u64_value(start_volume, reg_b2, p, offset_ptr);
            uint64_t last_price = start_price, last_volume = start_volume;

            for (size_t i = 0; i < ztime::MINUTES_IN_DAY; ++i) {
                const uint64_t timestamp = i * ztime::SECONDS_IN_MINUTE + start_timestamp;
                size_t sample_offset_ptr = offset_ptr + i * sample_size;
                bool status = false;
                check_no_value(status, reg_b1, p, sample_offset_ptr);
                if (!status) {
                    if (is_fill) {
                        // OHLCV
                        candles.emplace_back(0,0,0,0,0,timestamp);
                    }
                    continue;
                }
                int64_t cdo = 0, cdh = 0, cdl = 0, cdc = 0, cdv = 0;
                sample_offset_ptr = get_s64_value(cdo, reg_b1, p, sample_offset_ptr);
                sample_offset_ptr = get_s64_value(cdh, reg_b1, p, sample_offset_ptr);
                sample_offset_ptr = get_s64_value(cdl, reg_b1, p, sample_offset_ptr);
                sample_offset_ptr = get_s64_value(cdc, reg_b1, p, sample_offset_ptr);
                                    get_s64_value(cdv, reg_b3, p, sample_offset_ptr);
                const int64_t co = last_price + cdo;
                const int64_t ch = last_price + cdh;
                const int64_t cl = last_price + cdl;
                const int64_t cc = last_price + cdc;
                const int64_t cv = last_volume + cdv;
                last_price = cc;
                last_volume = cv;

                const double fco = (double)co / (double)price_factor;
                const double fch = (double)ch / (double)price_factor;
                const double fcl = (double)cl / (double)price_factor;
                const double fcc = (double)cc / (double)price_factor;
                const double fcv = (double)cv / (double)volume_factor;
                candles.emplace_back(fco,fch,fcl,fcc,fcv,timestamp);
            }
        } // read_sequence

        template<class T>
        inline void write_map(
                const T &candles,
                const size_t price_scale,
                const size_t volume_scale,
                const uint64_t timestamp_day = 0) {
            if (candles.empty()) return;
            auto it_begin = candles.begin();

            const double sh = it_begin->second.high;
            const double sl = it_begin->second.low;
            const double sc = it_begin->second.close;
            const double sv = it_begin->second.volume;
            const uint64_t st = it_begin->second.timestamp;

            double max_diff_price = std::max(std::abs(sh - sc), std::abs(sl - sc));
            double max_diff_volume = 0;
            double last_price = sc, last_volume = sv;
            for (const auto &c : candles) {
                // обновляем значение максимальной разницы цены
                max_diff_price = std::max(max_diff_price, std::max(std::abs(c.second.high - last_price), std::abs(c.second.low - last_price)));
                last_price = c.second.close;

                // обновляем значение максимальной разницы объема
                const double diff_volume = std::abs(c.second.volume - last_volume);
                max_diff_volume = std::max(max_diff_volume, diff_volume);
                last_volume = c.second.volume;
            }

            const uint64_t price_factor = (uint64_t)(std::pow(10, price_scale) + 0.5d);
            const uint64_t volume_factor = (uint64_t)(std::pow(10, volume_scale) + 0.5d);

            const uint64_t start_amplitude_price = (uint64_t)((sc * (double)price_factor) + 0.5d);
            const uint64_t start_amplitude_volume = (uint64_t)((sv * (double)volume_factor) + 0.5d);

            const uint64_t max_diff_amplitude_price = (uint64_t)((max_diff_price * (double)price_factor) + 0.5d);
            const uint64_t max_diff_amplitude_volume = (uint64_t)((max_diff_volume * (double)volume_factor) + 0.5d);

            const uint8_t reg_a0 = (uint8_t)(price_scale & 0x0F);
            const uint8_t reg_a1 = (uint8_t)(volume_scale & 0x0F);
            const uint8_t reg_a = reg_a0 | (reg_a1 << 4);

            // определяем тип переменной
            const uint8_t reg_b0 = calc_uint_type(start_amplitude_price);
            const uint8_t reg_b1 = calc_int_type(max_diff_amplitude_price);
            const uint8_t reg_b2 = calc_uint_type(start_amplitude_volume);
            const uint8_t reg_b3 = calc_int_type(max_diff_amplitude_volume);
            const uint8_t reg_b = (reg_b3 << 6) | (reg_b2 << 4) | (reg_b1 << 2) | (reg_b0 & 0x03);

            // определяем длину массива
            const size_t length = 2 +
                conv_int_type_to_bytes(reg_b0) + conv_int_type_to_bytes(reg_b2) +
                4 * conv_int_type_to_bytes(reg_b1) * ztime::MINUTES_IN_DAY +
                conv_int_type_to_bytes(reg_b3) * ztime::MINUTES_IN_DAY;

            data.resize(length);

            // записываем регистры
            data[0] = reg_a;
            data[1] = reg_b;
            if (timestamp_day == 0) {
                // находим unix день
                const uint16_t reg_c = ztime::get_day(st);
                data[2] = (uint8_t)reg_c;
                data[3] = (uint8_t)(reg_c >> 8);
            }

            uint8_t* p = data.data();
            size_t offset_ptr = 2;

            // записываем начальную цену
            offset_ptr = set_u64_value(start_amplitude_price, reg_b0, p, offset_ptr);
            // записываем начальный объем
            offset_ptr = set_u64_value(start_amplitude_volume, reg_b2, p, offset_ptr);
            const size_t sample_size = 4 * conv_int_type_to_bytes(reg_b1) + conv_int_type_to_bytes(reg_b3);

            // инициализируем отсутствие данных для всех баров
            for (size_t i = 0; i < ztime::MINUTES_IN_DAY; ++i) {
                size_t sample_offset_ptr = offset_ptr + i * sample_size;
                sample_offset_ptr = set_no_value(reg_b1, p, sample_offset_ptr);
                sample_offset_ptr = set_no_value(reg_b1, p, sample_offset_ptr);
                sample_offset_ptr = set_no_value(reg_b1, p, sample_offset_ptr);
                sample_offset_ptr = set_no_value(reg_b1, p, sample_offset_ptr);
                                    set_no_value(reg_b3, p, sample_offset_ptr);
            }

            // записываем те бары, что есть в наличии
            uint64_t last_cc = start_amplitude_price;
            uint64_t last_cv = start_amplitude_volume;
            for (auto &c : candles) {
                const int64_t co = (int64_t)((c.second.open * (double)price_factor) + 0.5d);
                const int64_t ch = (int64_t)((c.second.high * (double)price_factor) + 0.5d);
                const int64_t cl = (int64_t)((c.second.low * (double)price_factor) + 0.5d);
                const int64_t cc = (int64_t)((c.second.close * (double)price_factor) + 0.5d);
                const int64_t cv = (int64_t)((c.second.volume * (double)volume_factor) + 0.5d);

                const int64_t cdo = co - (int64_t)last_cc;
                const int64_t cdh = ch - (int64_t)last_cc;
                const int64_t cdl = cl - (int64_t)last_cc;
                const int64_t cdc = cc - (int64_t)last_cc;
                const int64_t cdv = cv - (int64_t)last_cv;

                size_t sample_offset_ptr = offset_ptr + sample_size * ztime::get_minute_day(c.second.timestamp);
                sample_offset_ptr = set_s64_value(cdo, reg_b1, p, sample_offset_ptr);
                sample_offset_ptr = set_s64_value(cdh, reg_b1, p, sample_offset_ptr);
                sample_offset_ptr = set_s64_value(cdl, reg_b1, p, sample_offset_ptr);
                sample_offset_ptr = set_s64_value(cdc, reg_b1, p, sample_offset_ptr);
                                    set_s64_value(cdv, reg_b3, p, sample_offset_ptr);
                last_cc = cc;
                last_cv = cv;
            }
        } // write_map

        /*
        template<class T>
        inline void read_map(
                T &candles,
                size_t &price_scale,
                size_t &volume_scale,
                const uint64_t timestamp_day = 0,
                const bool is_fill = false) {
            if (data.empty()) {
                price_scale = 0;
                volume_scale = 0;
                return;
            }

            // получаем точность котировок и объема
            price_scale = data[0] & 0x0F;			// reg_a 0-3
            volume_scale = (data[0] >> 4) & 0x0F;	// reg_a 4-7

            // получаем множитель для котировок иобъема
            const uint64_t price_factor = (uint64_t)(std::pow(10, price_scale) + 0.5d);
            const uint64_t volume_factor = (uint64_t)(std::pow(10, volume_scale) + 0.5d);

            // получаем количество байт для хранения данных
            const uint8_t reg_b0 = data[1] & 0x03;
            const uint8_t reg_b1 = (data[1] >> 2) & 0x03;
            const uint8_t reg_b2 = (data[1] >> 4) & 0x03;
            const uint8_t reg_b3 = (data[1] >> 6) & 0x03;

            // получаем метку времени
            const uint64_t start_timestamp = timestamp_day == 0 ? xtime::SECONDS_IN_DAY * (uint64_t)((((uint16_t)data[3]) << 8) | data[2]) : timestamp_day;

            const uint8_t* p = data.data();
            size_t offset_ptr = timestamp_day == 0 ? 4 : 2;

            const size_t sample_size = 4 * conv_int_type_to_bytes(reg_b1) + conv_int_type_to_bytes(reg_b3);

            // получаем стартовую цену и объем
            uint64_t start_price = 0, start_volume = 0;
            offset_ptr = get_u64_value(start_price, reg_b0, p, offset_ptr);
            offset_ptr = get_u64_value(start_volume, reg_b2, p, offset_ptr);
            uint64_t last_price = start_price, last_volume = start_volume;

            auto it = candles.begin();
            for (size_t i = 0; i < xtime::MINUTES_IN_DAY; ++i) {
                const uint64_t timestamp = i * xtime::SECONDS_IN_MINUTE + start_timestamp;
                size_t sample_offset_ptr = offset_ptr + i * sample_size;
                bool status = false;
                check_no_value(status, reg_b1, p, sample_offset_ptr);
                if (!status) {
                    if (is_fill) {
                        // OHLCV
                        candles[timestamp]
                        //candles.emplace_hint(it, timestamp, (0,0,0,0,0,timestamp));
                        it = candles.end();
                    }
                    continue;
                }
                int64_t cdo = 0, cdh = 0, cdl = 0, cdc = 0, cdv = 0;
                sample_offset_ptr = get_s64_value(cdo, reg_b1, p, sample_offset_ptr);
                sample_offset_ptr = get_s64_value(cdh, reg_b1, p, sample_offset_ptr);
                sample_offset_ptr = get_s64_value(cdl, reg_b1, p, sample_offset_ptr);
                sample_offset_ptr = get_s64_value(cdc, reg_b1, p, sample_offset_ptr);
                                    get_s64_value(cdv, reg_b3, p, sample_offset_ptr);
                const int64_t co = last_price + cdo;
                const int64_t ch = last_price + cdh;
                const int64_t cl = last_price + cdl;
                const int64_t cc = last_price + cdc;
                const int64_t cv = last_volume + cdv;
                last_price = cc;
                last_volume = cv;

                const double fco = (double)co / (double)price_factor;
                const double fch = (double)ch / (double)price_factor;
                const double fcl = (double)cl / (double)price_factor;
                const double fcc = (double)cc / (double)price_factor;
                const double fcv = (double)cv / (double)volume_factor;

                //candles.emplace_hint(it, timestamp, fco,fch,fcl,fcc,fcv,timestamp);
                it = candles.end();
            }
        } // read_sequence
        */
    };
};

#endif // TRADING_DB_QDB_COMPACT_CANDLES_DATASET_HPP_INCLUDED
