#pragma once
#ifndef TRADING_DB_ASYN_CTASKS_HPP_INCLUDED
#define TRADING_DB_ASYNC_TASKS_HPP_INCLUDED

#include <thread>
#include <atomic>
#include <mutex>
#include <future>
#include <deque>

namespace trading_db {
    namespace utility {
        /** \brief Класс для выполнений асинхронных задач
         */
        class AsyncTasks {
        private:
            std::mutex futures_mutex;
            std::deque<std::future<void>> futures;
            std::atomic<bool> is_shutdown = ATOMIC_VAR_INIT(false);

        public:

            /** \brief Очистить список запросов
             */
            void clear() noexcept {
                std::lock_guard<std::mutex> lock(futures_mutex);
                size_t index = 0;
                while(index < futures.size()) {
                    try {
                        if(futures[index].valid()) {
                            std::future_status status = futures[index].wait_for(std::chrono::milliseconds(0));
                            if(status == std::future_status::ready) {
                                futures[index].get();
                                futures.erase(futures.begin() + index);
                                continue;
                            }
                        }
                    }
                    catch(const std::exception &e) {}
                    catch(...) {}
                    ++index;
                }
            } // clear

            /** \brief Создать задачу
             * \param callback Функция с задачей, которую необходимо исполнить асинхронно
             */
            void create_task(const std::function<void()> &callback) noexcept {
                {
                    std::lock_guard<std::mutex> lock(futures_mutex);
                    futures.resize(futures.size() + 1);
                    futures.back() = std::async(std::launch::async, [callback] {
                        callback();
                    });
                }
                clear();
            } // creat_task

            inline bool check_shutdown() const noexcept {
                return is_shutdown;
            };

            inline void wait() {
                std::lock_guard<std::mutex> lock(futures_mutex);
                for(size_t i = 0; i < futures.size(); ++i) {
                    if(futures[i].valid()) {
                        try {
                            futures[i].wait();
                            futures[i].get();
                        } catch(...) {}
                    }
                }
            }

            AsyncTasks() noexcept {};

            ~AsyncTasks() noexcept {
                is_shutdown = true;
                std::lock_guard<std::mutex> lock(futures_mutex);
                for(size_t i = 0; i < futures.size(); ++i) {
                    if(futures[i].valid()) {
                        try {
                            futures[i].wait();
                            futures[i].get();
                        } catch(...) {}
                    }
                }
            } // ~AsyncTasks()

        }; // AsyncTasks
    }; // utility
}; // trading_db

#endif // TRADING_DB_ASYN_TASKS_HPP_INCLUDED
