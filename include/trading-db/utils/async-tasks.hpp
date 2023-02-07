#pragma once
#ifndef TRADING_DB_ASYNC_TASKS_HPP_INCLUDED
#define TRADING_DB_ASYNC_TASKS_HPP_INCLUDED

#include <thread>
#include <atomic>
#include <mutex>
#include <future>
#include <deque>

namespace trading_db {
	namespace utils {

		/** \brief Класс для выполнений асинхронных задач
		 */
		class AsyncTasks {
		private:
			std::mutex						futures_mutex;
			std::deque<std::future<void>>	futures;
			std::atomic<bool>				is_shutdown = ATOMIC_VAR_INIT(false);
			std::atomic<int>				counter = ATOMIC_VAR_INIT(0);

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
					futures.back() = std::async(std::launch::async, [&, callback] {
						counter += 1;
						callback();
						counter -= 1;
					});
				}
				clear();
			} // creat_task

			/** \brief Проверить флаг сброса
			 * \return Вернет true в случае наличия сброса
			 */
			inline bool check_shutdown() const noexcept {
				return is_shutdown;
			};

			/** \brief Ожидание завершения всех задач
			 */
			inline void wait() {
				/*
				std::lock_guard<std::mutex> lock(futures_mutex);
				for(size_t i = 0; i < futures.size(); ++i) {
					std::shared_future<void> share = futures[i].share();
					if(share.valid()) {
						try {
							share.wait();
							share.get();
						} catch(...) {}
					}
				}
				*/
				size_t index = 0;
				while (!false) {
					std::unique_lock<std::mutex> locker(futures_mutex);
					if (index < futures.size()) {
						try {
							std::shared_future<void> share = futures[index].share();
							if (share.valid()) {
								if (share.wait_for(std::chrono::milliseconds(1)) ==
									std::future_status::timeout) {
									locker.unlock();
									std::this_thread::sleep_for(std::chrono::milliseconds(1));
									continue;
								}
								share.get();
								futures.erase(futures.begin() + index);
								continue;
							}
						}
						catch(const std::exception &e) {}
						catch(...) {}
						++index;
					} else break;
				}
			}

			/** \brief Проверить занятость задачами
			 * \return Вернет true, если есть хотя бы одна не выполненная задача
			 */
			inline bool busy() noexcept {
				if (counter == 0) return false;
				return true;
			} // busy

			AsyncTasks() {};

			~AsyncTasks() {
				is_shutdown = true;
				/*
				std::lock_guard<std::mutex> lock(futures_mutex);
				for(size_t i = 0; i < futures.size(); ++i) {
					if(futures[i].valid()) {
						try {
							futures[i].wait();
							futures[i].get();
						} catch(...) {}
					}
				}
				*/
				std::lock_guard<std::mutex> locker(futures_mutex);
				for(size_t i = 0; i < futures.size(); ++i) {
					std::shared_future<void> share = futures[i].share();
					if(share.valid()) {
						try {
							share.wait();
							share.get();
						} catch(...) {}
					}
				}
			} // ~AsyncTasks()

		}; // AsyncTasks
	}; // utils
}; // trading_db

#endif // TRADING_DB_ASYNC_TASKS_HPP_INCLUDED
