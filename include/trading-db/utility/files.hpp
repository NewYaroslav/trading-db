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
	}; // utility
}; // mega_connector

#endif // MEGA_CONNECTOR_FILES_HPP_INCLUDED
