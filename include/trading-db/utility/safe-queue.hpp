#pragma once
#ifndef TRADING_DB_UTILITY_SAFE_QUEUE_HPP_INCLUDED
#define TRADING_DB_UTILITY_SAFE_QUEUE_HPP_INCLUDED

#include <mutex>
#include <condition_variable>
#include <queue>

namespace trading_db {
	namespace utility {

		template <class T>
		class SafeQueue {
		public:

			SafeQueue() : q(), m(), c() {}

			~SafeQueue() {
				reset();
			}

			// Add an element to the queue.
			inline void enqueue(const T &v) noexcept {
				std::unique_lock<std::mutex> locker(m);
				q.push(v);
				c.notify_one();
			}

			// Get the "front"-element.
			// If the queue is empty, wait till a element is avaiable.
			inline T dequeue(void) noexcept {
				std::unique_lock<std::mutex> locker(m);
				c.wait(locker, [&](){return !q.empty() || shutdown;});
				if (!q.empty()) {
					auto val = q.front();
					q.pop();
					return val;
				} else {
					T val;
					return val;
				}
			}

			inline void update(const std::function<void(const T &value)> &on_item) noexcept {
				{
					std::unique_lock<std::mutex> locker(m);
					c.wait(locker, [&](){return !q.empty() || shutdown;});
					shutdown = false;
				}
				while (!false) {
					T value;
					{
						std::unique_lock<std::mutex> locker(m);
						if (q.empty()) return;
						value = q.front();
						q.pop();
					}
					on_item(value);
				}
			}

			inline bool update(
					const size_t delay_ms,
					const std::function<void(const T &value)> &on_item,
					const std::function<void()> &on_reset = nullptr,
					const std::function<void()> &on_timeout = nullptr) noexcept {
				{
					std::unique_lock<std::mutex> locker(m);
					if (	!c.wait_for(
							locker,
							std::chrono::milliseconds(delay_ms),
							[&](){return !q.empty() || shutdown;})) {
						if (on_timeout) on_timeout();
						return false;
					}
				}
				if (shutdown) {
					if (on_reset) on_reset();
					shutdown = false;
				}
				while (!false) {
					T value;
					{
						std::unique_lock<std::mutex> locker(m);
						if (q.empty()) return true;
						value = q.front();
						q.pop();
					}
					on_item(value);
				}
			}

			inline void update() noexcept {
				{
					std::unique_lock<std::mutex> locker(m);
					c.wait(locker, [&](){return !q.empty() || shutdown;});
					shutdown = false;
				}
				while (!false) {
					T value;
					{
						std::unique_lock<std::mutex> locker(m);
						if (q.empty()) return;
						value = q.front();
						q.pop();
					}
					value();
				}
			}

			inline void add(const T &value) noexcept {
				enqueue(value);
			}

			inline void reset() noexcept {
				shutdown = true;
				c.notify_one();
			}

		private:
			std::queue<T> q;
			mutable std::mutex m;
			std::condition_variable c;
			std::atomic<bool> shutdown = ATOMIC_VAR_INIT(false);
		};
	};
};

#endif // TRADING_DB_UTILITY_SAFE_QUEUE_HPP_INCLUDED
