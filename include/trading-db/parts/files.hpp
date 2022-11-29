#ifndef MEGA_CONNECTOR_FILES_HPP_INCLUDED
#define MEGA_CONNECTOR_FILES_HPP_INCLUDED

#include <fstream>
//#include <stdlib.h>
#include <dirent.h>
#include <dir.h>
//#include <conio.h>
#include <vector>

namespace trading_db {
	namespace utility {

		/** \brief Load whole file into std :: string
		 * \param file_name File name
		 * \param file_data File data
		 * \return Вернет true, если функция загрузил данные
		 */
		bool load_file(const std::string &file_name, std::string &file_data) {
			std::ifstream file(file_name, std::ios_base::binary);
			if(!file) return false;
			file.seekg(0, std::ios_base::end);
			std::ifstream::pos_type file_size = file.tellg();
			file.seekg(0);
			file_data.reserve(file_size);
			char temp;
			while(file.read((char*)&temp, sizeof(char))) file_data.push_back(temp);
			file.close();
			return !file_data.empty();
		}

		/** \brief Загрузить файл целиком в std::string
		 * \param file_name имя файла
		 * \param buffer данные файла
		 * \param buffer_size размер буфера
		 * \param start_buffer_pos начало записи в буфер
		 * \return размер файла, вернет -1 если файл не удалось открыть
		 */
		int load_file(std::string file_name, void *buffer, size_t buffer_size, size_t start_buffer_pos = 0) {
				std::ifstream file(file_name, std::ios_base::binary);
				if (!file) return -1;
				file.seekg(0, std::ios_base::end);
				std::ifstream::pos_type file_size = file.tellg();
				file.seekg(0);
				if ((size_t)file_size > (buffer_size - start_buffer_pos)) return -1;
				file.read((char*)buffer + start_buffer_pos, file_size);
				return file_size;
		}

		/** \brief Записать файл целиком из буфера
		 * \param file_name имя файла
		 * \param buffer буфер
		 * \param buffer_size размер буфера
		 * \return размер файла, вернет -1 если файл не удалось записать
		 */
		int write_file(std::string file_name, void *buffer, size_t buffer_size) {
			std::ofstream file(file_name, std::ios_base::binary);
			if (!file) return -1;
			file.write((char*)buffer, buffer_size);
			file.close();
			return buffer_size;
		}

		/** \brief Получить размер файла
		 * \param file_name имя файла
		 * \return размер файла, вернет -1 если файл не удалось открыть
		 */
		int get_file_size(std::string file_name) {
			std::ifstream file;
			file.open(file_name);
			if (!file) return -1;
			file.seekg(0, std::ios::end);
			int file_size = file.tellg();
			file.close();
			return file_size;
		}

		/** \brief Разобрать путь на составляющие
		 * Данная функция парсит путь, например C:/Users\\user/Downloads разложит на
		 * C:, Users, user и Downloads
		 * \param path путь, который надо распарсить
		 * \param output_list список полученных элементов
		 */
		void parse_path(std::string path, std::vector<std::string> &output_list) {
			if(path.back() != '/' && path.back() != '\\') path += "/";
			std::size_t start_pos = 0;
			while(true) {
				std::size_t found_beg = path.find_first_of("/\\~", start_pos);
				if(found_beg != std::string::npos) {
					std::size_t len = found_beg - start_pos;
					if(len > 0) {
						if(output_list.size() == 0 && path.size() > 3 && path.substr(0, 2) == "~/") {
							output_list.push_back(path.substr(0, 2));
						} else
						if(output_list.size() == 0 && path.size() > 2 && path.substr(0, 1) == "/") {
							output_list.push_back(path.substr(0, 1));
						}
						output_list.push_back(path.substr(start_pos, len));
					}
					start_pos = found_beg + 1;
				} else break;
			}
		}

		/** \brief Установить или изменить расширение файла
		 * \param path Путь к файлу
		 * \param file_extension новое расширение файла
		 * \param separator Символ разделителя
		 * \return Вернет имя файла с указанным расширением
		 */
		std::string set_file_extension(const std::string path, const std::string file_extension, const std::string separator = "/") {
			if (!path.size()) return (path + file_extension);
			std::vector<std::string> elemet_list;
			parse_path(path, elemet_list);
			std::string file_name = elemet_list.back();
			std::size_t found_point = file_name.find_last_of(".");
			if (found_point == std::string::npos) return (path + file_extension);
			std::string _file_extension = file_name.substr(found_point);
			std::string _file_name = file_name.substr(0, found_point);
			std::string new_path;
			for(size_t i = 0; i < elemet_list.size() - 1; ++i) {
				new_path += elemet_list[i] + separator;
			}
			new_path += _file_name + file_extension;
			return new_path;
		}

		/** \brief Создать директорию
		 * \param path директория, которую необходимо создать
		 * \param is_file Флаг, по умолчанию false. Если указать true, последний раздел пути будет являться именем файла и не будет считаться "папкой".
		 */
		void create_directory(std::string path, const bool is_file = false) {
			std::vector<std::string> dir_list;
			parse_path(path, dir_list);
			std::string name;
			size_t dir_list_size = is_file ? dir_list.size() - 1 : dir_list.size();
			for(size_t i = 0; i < dir_list_size; i++) {
				if(i > 0) name += dir_list[i] + "/";
				else if(i == 0 &&
					(dir_list[i] == "/" ||
					dir_list[i] == "~/")) {
					name += dir_list[i];
				} else name += dir_list[i] + "/";
				if (dir_list[i] == ".." ||
					dir_list[i] == "/" ||
					dir_list[i] == "~/") continue;
				mkdir(name.c_str());
			}
		}

		/** \brief Получить список файлов
		 * \param path путь, с которого начинается поиск
		 * \param output_list список файлов
		 * \param is_with_directory данный параметр влияет сразу на два фактора.
		 * Если параметр false, список формируется только из имен файлов, которые есть в path
		 * Поиск файлов во вложенных папках при этом не производится
		 * Если параметр указан как true, тогда имя файла формируется с учетом path, а также
		 * функция делает поиск во всех вложенных папках
		 * \param is_folders_only Данная функция будет помещать в спсиок только папки
		 * \return количество найденых файлов
		 */

		unsigned long get_list_files(
				std::string path,
				std::vector<std::string> &output_list,
				const bool is_with_directory = true,
				const bool is_folders_only = false) {
			DIR* directory;									   // Directory object
			dirent* directory_name;
			unsigned long num_files = 0;

			if (path.back() != '/' && path.back() != '\\') path += "/";
			if ((directory = opendir(path.c_str()))) {
				// открываем директорию
				while ((directory_name = readdir(directory))) {
					//unsigned long str_len = strlen(directory_name->d_name);
					char * search_char = (char*) memchr(directory_name->d_name, '.', strlen(directory_name->d_name));
					std::string str_directory_name = std::string(directory_name->d_name);
					if (search_char == NULL) {
						if (is_folders_only && is_with_directory) {
							output_list.push_back(path + str_directory_name);
						} else
						if (is_folders_only && !is_with_directory) {
							output_list.push_back(str_directory_name);
						}
						if (is_with_directory) {
							num_files += get_list_files(path + str_directory_name, output_list, is_with_directory, is_folders_only);
						}
						continue;
					}
					if (str_directory_name == "." ||
						str_directory_name == ".." ||
						is_folders_only) continue;
					std::string file_name;
					if (is_with_directory) output_list.push_back(path + str_directory_name);
					else output_list.push_back(str_directory_name);
					num_files++;
				}
			}
			closedir(directory);
			return num_files;
		}
	}; // utility
}; // mega_connector

#endif // MEGA_CONNECTOR_FILES_HPP_INCLUDED
