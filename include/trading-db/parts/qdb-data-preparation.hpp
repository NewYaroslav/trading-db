#pragma once
#ifndef TRADING_DB_QDB_DATA_PREPARATION_HPP_INCLUDED
#define TRADING_DB_QDB_DATA_PREPARATION_HPP_INCLUDED

#include "qdb-compact-dataset.hpp"
#include "qdb-common.hpp"
#include "qdb-dictionary-candles.hpp"
#include "qdb-dictionary-ticks.hpp"
#include <map>
#include <vector>
#include "zdict.h"
#include "zstd.h"

namespace trading_db {

	/** \brief Класс для подготовки данных
	 */
	class QdbDataPreparation {
	public:

		// общие параметры работы с данными цен
		class Config {
		public:
			// количество знаков после запятой
			size_t	price_scale				= 0;
			// флаг заполнения данных
			bool	use_filling_empty_bars = false;

			// уровень сжатия
			int		compress_level			= ZSTD_maxCLevel();

			// указатели на данные библиотек для сжатия
			uint8_t *dictionary_candles_ptr = nullptr;
			size_t	dictionary_candles_size = 0;
			uint8_t *dictionary_ticks_ptr	= nullptr;
			size_t	dictionary_ticks_size	= 0;
		} config;

	private:

		// сжимаем сырые данные
		inline bool compress_raw_data(
				const uint8_t *dict_ptr,
				const size_t dict_size,
				const std::vector<uint8_t> &src,
				std::vector<uint8_t> &dst) noexcept {

			size_t new_compressed_data_size = ZSTD_compressBound(src.size());
			dst.resize(new_compressed_data_size);

			ZSTD_CCtx* const cctx = ZSTD_createCCtx();
			size_t compressed_size = ZSTD_compress_usingDict(
				cctx,
				dst.data(),
				dst.size(),
				src.data(),
				src.size(),
				dict_ptr,
				dict_size,
				config.compress_level);

			ZSTD_freeCCtx(cctx);
			if (ZSTD_isError(compressed_size)) {
				return false;
			}
			dst.resize(compressed_size);

			//std::cout << "src size: " << src.size() << " dst size: " << dst.size() << std::endl;
			return true;
		}

		// разорхивируем сырые данные
		inline bool decompress_raw_data(
				const uint8_t *dict_ptr,
				const size_t dict_size,
				const std::vector<uint8_t> &src,
				std::vector<uint8_t> &dst) noexcept {

			const unsigned long long raw_decompress_size = ZSTD_getFrameContentSize(src.data(), src.size());
			if(raw_decompress_size == ZSTD_CONTENTSIZE_ERROR) {
				//std::cout << "error ZSTD_CONTENTSIZE_ERROR!" << std::endl;
				return false;
			} else
			if(raw_decompress_size == ZSTD_CONTENTSIZE_UNKNOWN) {
				//std::cout << "error ZSTD_CONTENTSIZE_UNKNOWN!" << std::endl;
				return false;
			}

			dst.resize(raw_decompress_size);

			ZSTD_DCtx* const dctx = ZSTD_createDCtx();
			const size_t decompress_size = ZSTD_decompress_usingDict(
				dctx,
				dst.data(),
				raw_decompress_size,
				src.data(),
				src.size(),
				dict_ptr,
				dict_size);

			ZSTD_freeDCtx(dctx);
			if (ZSTD_isError(decompress_size)) {
				//std::cout << "error ZSTD_isError!" << std::endl;
				//std::cout << "error decompressin: " << ZSTD_getErrorName(decompress_size) << std::endl;
				return false;
			}
			dst.resize(decompress_size);
			return true;
		}

	public:

		QdbDataPreparation() {
			config.dictionary_candles_ptr = (uint8_t *)qdb_dictionary_candles;
			config.dictionary_candles_size = sizeof(qdb_dictionary_candles);
			config.dictionary_ticks_ptr = (uint8_t *)qdb_dictionary_ticks;
			config.dictionary_ticks_size = sizeof(qdb_dictionary_ticks);
		}

		~QdbDataPreparation(){

		}

		inline bool compress_candles(
				const std::array<Candle, ztime::MINUTES_IN_DAY> &src,
				std::vector<uint8_t> &dst) noexcept {
			trading_db::QdbCompactDataset dataset;
			dataset.write_candles(src, config.price_scale, 0);
			auto &data = dataset.get_data();
			return compress_raw_data((const uint8_t *)config.dictionary_candles_ptr, config.dictionary_candles_size, data, dst);
		}

		inline bool decompress_candles(
				const uint64_t timestamp_day,
				const std::vector<uint8_t> &src,
				std::array<Candle, ztime::MINUTES_IN_DAY> &dst) noexcept {
			trading_db::QdbCompactDataset dataset;
			auto &data = dataset.get_data();
			if (!decompress_raw_data((const uint8_t *)config.dictionary_candles_ptr, config.dictionary_candles_size, src, data)) {
				return false;
			}
			size_t volume_scale = 0;
			dataset.read_candles(dst, config.price_scale, volume_scale, timestamp_day, config.use_filling_empty_bars);
			return true;
		}

		inline bool compress_ticks(
				const uint64_t timestamp_hour,
				const std::map<uint64_t, trading_db::ShortTick> &src,
				std::vector<uint8_t> &dst) noexcept {
			const uint64_t t_ms = timestamp_hour * ztime::MILLISECONDS_IN_SECOND;
			trading_db::QdbCompactDataset dataset;
			dataset.write_ticks(src, config.price_scale, t_ms);
			auto &data = dataset.get_data();
			return compress_raw_data(config.dictionary_ticks_ptr, config.dictionary_ticks_size, data, dst);
		}

		inline bool decompress_ticks(
				const uint64_t timestamp_hour,
				const std::vector<uint8_t> &src,
				std::map<uint64_t, trading_db::ShortTick> &dst) noexcept {
			const uint64_t t_ms = timestamp_hour * ztime::MILLISECONDS_IN_SECOND;
			trading_db::QdbCompactDataset dataset;
			auto &data = dataset.get_data();
			if (!decompress_raw_data(config.dictionary_ticks_ptr, config.dictionary_ticks_size, src, data)) {
				return false;
			}
			dataset.read_ticks(dst, config.price_scale, t_ms);
			return true;
		}
	}; // QdbDataPreparation
}; // trading_db

#endif // TRADING_DB_QDB_DATA_PREPARATION_HPP_INCLUDED
