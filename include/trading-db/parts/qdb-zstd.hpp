#pragma once
#ifndef TRADING_DB_QDB_ZSTD_HPP_INCLUDED
#define TRADING_DB_QDB_ZSTD_HPP_INCLUDED

#include <functional>
#include <map>
#include <string>
#include <vector>
#include "zdict.h"
#include "zstd.h"
#include "ztime.hpp"
#include "qdb-common.hpp"
#include "files.hpp"

namespace trading_db {

	std::string convert_hex_to_string(unsigned char value) {
		char hex_string[32] = {};
		sprintf(hex_string,"0x%.2X", value);
		return std::string(hex_string);
	}

	std::string str_toupper(std::string s) {
		std::transform(s.begin(), s.end(), s.begin(),
			[](unsigned char c){ return std::toupper(c); });
		return s;
	}

	std::string str_tolower(std::string s) {
		std::transform(s.begin(), s.end(), s.begin(),
			[](unsigned char c){ return std::tolower(c); });
		return s;
	}

	/** \brief Тренируйте словарь из массива образцов
	 * \param files_list Список файлов для обучения
	 * \param path путь к файлу словаря
	 * \param name имя словаря
	 * \param dict_buffer_capacit размер словаря
	 * \param is_file Флаг файла словаря. Если установлен, то словарь будет сохранен как бинарный файл
	 * \return венет 0 в случае успеха
	 */
	bool train_zstd(
			const std::vector<std::string> &files_list,
			const std::string &path,
			const std::string &name,
			const size_t &dict_buffer_capacit = 102400,
			const bool is_file = true) {

		// буферы для подготовки образцов обучения
		size_t all_files_size = 0;
		void *samples_buffer = NULL;
		size_t num_files = 0;
		size_t *samples_size = NULL;

		// выясним, сколько нам нужно выделить памяти под образцы
		unsigned long files_size = 0;
		unsigned long files_counter = 0;
		for (size_t i = 0; i < files_list.size(); ++i) {
			unsigned long file_size = utility::get_file_size(files_list[i]);
			if(file_size > 0) {
				files_size += file_size;
				files_counter++;
			}
		}

		// выделим память под образцы
		samples_buffer = malloc(files_size);
		samples_size = (size_t*)malloc(files_counter * sizeof(size_t));

		// заполним буферы образцов
		for (size_t i = 0; i < files_list.size(); ++i) {
			unsigned long file_size = utility::get_file_size(files_list[i]);
			if (file_size > 0) {
				num_files++;
				size_t start_pos = all_files_size;
				all_files_size += file_size;
				samples_size[num_files - 1] = file_size;

				unsigned long err = utility::load_file(files_list[i], samples_buffer, all_files_size, start_pos);
				if (err != file_size) {
					std::cout << "read file error, file: " << files_list[i] << " size: " << err << std::endl;
					if (samples_buffer) free(samples_buffer);
					if (samples_size) free(samples_size);
					return false;
				} else {
					std::cout << "file: " << files_list[i] << " #" << num_files << "/" << files_list.size() << "\r";
				}
			} else {
				std::cout << "buffer size: get file size error, file: " << files_list[i] << " size: " << file_size << std::endl;
				if (samples_buffer) free(samples_buffer);
				if (samples_size) free(samples_size);
				return false;
			}
		}
		std::cout << std::endl;

		void *dict_buffer = NULL;
		dict_buffer = malloc(dict_buffer_capacit);
		memset(dict_buffer, 0, dict_buffer_capacit);
		size_t file_size = ZDICT_trainFromBuffer(dict_buffer, dict_buffer_capacit, samples_buffer, samples_size, num_files);

		if (is_file) {
			size_t err = utility::write_file(path, dict_buffer, file_size);
			return (err > 0);
		} else {
			std::string dictionary_name_upper = str_toupper(name);
			std::string dictionary_name_lower = str_tolower(name);

			// сохраним как с++ файл
			unsigned char *dict_buffer_point = (unsigned char *)dict_buffer;
			std::string out;
			out += "#ifndef DICTIONARY_" + dictionary_name_upper+ "_HPP_INCLUDED\n";
			out += "#define DICTIONARY_" + dictionary_name_upper + "_HPP_INCLUDED\n";
			out += "\n";
			out += "namespace zstd_dictionary {\n";
			out += "\tconst static unsigned char " + dictionary_name_lower + "[" + std::to_string(file_size) + "] = {\n";
			out += "\t\t";
			for (size_t j = 0; j < file_size; ++j) {
				if(j > 0 && (j % 16) == 0) {
					out += "\n\t\t";
				}
				out += convert_hex_to_string(dict_buffer_point[j]) + ",";
				if(j == file_size - 1) {
					out += "\n\t};\n";
				}
			}
			out += "}\n";
			out += "#endif // DICTIONARY_" + dictionary_name_upper + "_HPP_INCLUDED\n";
			std::string path_out = utility::set_file_extension(path, ".hpp");
			size_t err = utility::write_file(path_out, (void*)out.c_str(), out.size());
			return (err > 0);
		}
	}

	/** \brief Тренируйте словарь из массива образцов
	* \param path путь к файлам
	* \param file_name имя файл словаря, который будет сохранен по окончанию обучения
	* \param dict_buffer_capacit размер словаря
	* \return венет 0 в случае успеха
	*/
	int train_zstd(
			std::vector<std::string> &files_list,
			std::string file_name,
			size_t dict_buffer_capacit = 102400) {
		size_t all_files_size = 0;
		void *samples_buffer = NULL;

		size_t num_files = 0;
		size_t *samples_size = NULL;
		for(size_t i = 0; i < files_list.size(); ++i) {
			int file_size = utility::get_file_size(files_list[i]);
			if(file_size > 0) {
				num_files++;
				size_t start_pos = all_files_size;
				all_files_size += file_size;
				std::cout << "buffer size: " << all_files_size << std::endl;
				samples_buffer = realloc(samples_buffer, all_files_size);
				samples_size = (size_t*)realloc((void*)samples_size, num_files * sizeof(size_t));
				samples_size[num_files - 1] = file_size;
				int err = utility::load_file(files_list[i], samples_buffer, all_files_size, start_pos);
				if(err != file_size) {
					std::cout << "load file: error, " << files_list[i] << std::endl;
					if(samples_buffer != NULL)
						free(samples_buffer);
					if(samples_size != NULL)
						free(samples_size);
					return false;
				} else {
					std::cout << "load file: " << files_list[i] << " #" << num_files << "/" << files_list.size() << std::endl;
				}
			} else {
				std::cout << "buffer size: error, " << files_list[i] << std::endl;
				if(samples_buffer != NULL)
					free(samples_buffer);
				if(samples_size != NULL)
					free(samples_size);
				return false;
			}
		}
		void *dict_buffer = NULL;
		dict_buffer = malloc(dict_buffer_capacit);
		memset(dict_buffer, 0, dict_buffer_capacit);
		size_t file_size = ZDICT_trainFromBuffer(dict_buffer, dict_buffer_capacit, samples_buffer, samples_size, num_files);
		size_t err = utility::write_file(file_name, dict_buffer, file_size);
		return (err > 0);
	}

	/** \brief Тренируйте словарь из массива образцов
	 * \param path путь к файлам
	 * \param file_name имя файл словаря, который будет сохранен по окончанию обучения
	 * \return венет 0 в случае успеха
	 */
	bool train_zstd(std::string path, std::string file_name, size_t dict_buffer_capacit = 102400) {
		std::vector<std::string> files_list;
		utility::get_list_files(path, files_list, true);
		return train_zstd(files_list, file_name, dict_buffer_capacit);
	}

	bool train_zstd(const std::string &path, const std::string &file_name, const std::string &dict_name, size_t dict_buffer_capacit = 102400) {
		std::vector<std::string> files_list;
		utility::get_list_files(path, files_list, true);
		return train_zstd(files_list, file_name, dict_name, dict_buffer_capacit, false);
	}

	/** \brief Сжать файл с использованием словаря
	 * \param input_file файл, который надо сжать
	 * \param output_file файл, в который сохраним данные
	 * \param dictionary_file файл словаря для сжатия
	 * \param compress_level уровень сжатия файла
	 * \return вернет 0 в случае успеха
	 */
	int compress_file(
			std::string input_file,
			std::string output_file,
			std::string dictionary_file,
			int compress_level = ZSTD_maxCLevel()) {
		int input_file_size = utility::get_file_size(input_file);
		if(input_file_size <= 0) return false;

		int dictionary_file_size = utility::get_file_size(dictionary_file);
		if(dictionary_file_size <= 0) return false;

		void *input_file_buffer = NULL;
		input_file_buffer = malloc(input_file_size);
		memset(input_file_buffer, 0, input_file_size);

		void *dictionary_file_buffer = NULL;
		dictionary_file_buffer = malloc(dictionary_file_size);
		memset(dictionary_file_buffer, 0, dictionary_file_size);

		utility::load_file(dictionary_file, dictionary_file_buffer, dictionary_file_size);
		utility::load_file(input_file, input_file_buffer, input_file_size);

		size_t compress_file_size = ZSTD_compressBound(input_file_size);
		void *compress_file_buffer = NULL;
		compress_file_buffer = malloc(compress_file_size);
		memset(compress_file_buffer, 0, compress_file_size);

		ZSTD_CCtx* const cctx = ZSTD_createCCtx();
		size_t compress_size = ZSTD_compress_usingDict(
			cctx,
			compress_file_buffer,
			compress_file_size,
			input_file_buffer,
			input_file_size,
			dictionary_file_buffer,
			dictionary_file_size,
			compress_level
			);

		if(ZSTD_isError(compress_size)) {
			std::cout << "error compressin: " << ZSTD_getErrorName(compress_size) << std::endl;
			ZSTD_freeCCtx(cctx);
			free(compress_file_buffer);
			free(dictionary_file_buffer);
			free(input_file_buffer);
			return false;
		}

		utility::write_file(output_file, compress_file_buffer, compress_size);

		ZSTD_freeCCtx(cctx);
		free(compress_file_buffer);
		free(dictionary_file_buffer);
		free(input_file_buffer);
		return true;
	}

	/** \brief Декомпрессия файла
	 * \param input_file сжатый файл
	 * \param output_file файл, в который сохраним данные
	 * \param dictionary_file файл словаря для декомпресии
	 * \return вернет 0 в случае успеха
	 */
	int decompress_file(
			std::string input_file,
			std::string output_file,
			std::string dictionary_file) {
		int input_file_size = utility::get_file_size(input_file);
		if (input_file_size <= 0)
			return false;

		int dictionary_file_size = utility::get_file_size(dictionary_file);
		if (dictionary_file_size <= 0)
			return false;

		void *input_file_buffer = NULL;
		input_file_buffer = malloc(input_file_size);
		memset(input_file_buffer, 0, input_file_size);

		void *dictionary_file_buffer = NULL;
		dictionary_file_buffer = malloc(dictionary_file_size);
		memset(dictionary_file_buffer, 0, dictionary_file_size);

		utility::load_file(dictionary_file, dictionary_file_buffer, dictionary_file_size);
		utility::load_file(input_file, input_file_buffer, input_file_size);

		unsigned long long const decompress_file_size = ZSTD_getFrameContentSize(input_file_buffer, input_file_size);
		if (decompress_file_size == ZSTD_CONTENTSIZE_ERROR) {
			std::cout << input_file << " it was not compressed by zstd." << std::endl;
			free(dictionary_file_buffer);
			free(input_file_buffer);
			return false;
		} else
		if (decompress_file_size == ZSTD_CONTENTSIZE_UNKNOWN) {
			std::cout << input_file << " original size unknown." << std::endl;
			free(dictionary_file_buffer);
			free(input_file_buffer);
			return false;
		}
		void *decompress_file_buffer = NULL;
		decompress_file_buffer = malloc(decompress_file_size);
		memset(decompress_file_buffer, 0, decompress_file_size);

		ZSTD_DCtx* const dctx = ZSTD_createDCtx();
		size_t const decompress_size = ZSTD_decompress_usingDict(
			dctx,
			decompress_file_buffer,
			decompress_file_size,
			input_file_buffer,
			input_file_size,
			dictionary_file_buffer,
			dictionary_file_size);

		if(ZSTD_isError(decompress_size)) {
			std::cout << "error decompressin: " << ZSTD_getErrorName(decompress_size) << std::endl;
			ZSTD_freeDCtx(dctx);
			free(decompress_file_buffer);
			free(dictionary_file_buffer);
			free(input_file_buffer);
			return false;
		}

		utility::write_file(output_file, decompress_file_buffer, decompress_size);

		ZSTD_freeDCtx(dctx);
		free(decompress_file_buffer);
		free(dictionary_file_buffer);
		free(input_file_buffer);
		return true;
	}
};

#endif // TRADING_DB_QDB_ZSTD_HPP_INCLUDED
