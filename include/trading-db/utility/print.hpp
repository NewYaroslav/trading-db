#ifndef PRINT_HPP_INCLUDED
#define PRINT_HPP_INCLUDED

#include <thread>
#include <mutex>
#include <sstream>

namespace trading_db {

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

}; // trading_db

#endif // PRINT_HPP_INCLUDED
