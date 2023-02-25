#pragma once
#ifndef TRADING_DB_UTILS_SAFE_QUEUE_HPP_INCLUDED
#define TRADING_DB_UTILS_SAFE_QUEUE_HPP_INCLUDED

#include <mutex>
#include <condition_variable>
#include <queue>

namespace trading_db {
    namespace utils {

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
                if (!q.empty()) return T();
                auto val = q.front();
                q.pop();
                locker.unlock();
                return val;
            }

            inline void update(const std::function<void(const T &value)> &on_item) noexcept {
                std::unique_lock<std::mutex> locker(m);
                c.wait(locker, [&](){return !q.empty() || shutdown;});
                shutdown = false;
                auto data_queue = std::move(q);
                locker.unlock();
                while (!data_queue.empty()) {
                    on_item(data_queue.front());
                    data_queue.pop();
                }
            }

            inline bool update(
                    const size_t delay_ms,
                    const std::function<void(const T &value)> &on_item,
                    const std::function<void()> &on_reset = nullptr,
                    const std::function<void()> &on_timeout = nullptr) noexcept {
                std::unique_lock<std::mutex> locker(m);
                bool is_timeout = c.wait_for(
                    locker,
                    std::chrono::milliseconds(delay_ms),
                    [&](){return !q.empty() || shutdown;});
                auto data_queue = std::move(q);
                locker.unlock();

                if (!is_timeout) {
                    if (on_timeout) on_timeout();
                    return false;
                }
                if (shutdown) {
                    if (on_reset) on_reset();
                    shutdown = false;
                }
                while (!data_queue.empty()) {
                    on_item(data_queue.front());
                    data_queue.pop();
                }
            }

            inline void update() noexcept {
                std::unique_lock<std::mutex> locker(m);
                c.wait(locker, [&](){return !q.empty() || shutdown;});
                shutdown = false;
                auto data_queue = std::move(q);
                locker.unlock();
                while (!data_queue.empty()) {
                    T &value = q.front();
                    value();
                    data_queue.pop();
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

#endif // TRADING_DB_UTILS_SAFE_QUEUE_HPP_INCLUDED
