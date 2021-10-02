#pragma once
#ifndef TRADING_DB_UTILITY_PRINT_HPP_INCLUDED
#define TRADING_DB_UTILITY_PRINT_HPP_INCLUDED

#include <thread>
#include <mutex>
#include <sstream>

namespace trading_db {
    namespace utility {

        /** \brief Многопоточная версия вывода в консоль
         */
        class PrintThread: public std::ostringstream {
        private:
            static inline std::mutex _mutexPrint;

        public:
            PrintThread() = default;

            ~PrintThread() {
                std::lock_guard<std::mutex> guard(_mutexPrint);
                std::cout << this->str();
            }
        };
    }; // utility
}; // trading_db

#endif // TRADING_DB_UTILITY_PRINT_HPP_INCLUDED
