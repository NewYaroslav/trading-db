#pragma once
#ifndef TRADING_DB_PROTECTION_HPP_INCLUDED
#define TRADING_DB_PROTECTION_HPP_INCLUDED

namespace trading_db {
	namespace utils {

		/*
		inline void create_protected(
				const std::function<void()> &callback,
				const std::function<void(const uint64_t counter)> &callback_error,
				const uint64_t delay = 500) {
			uint64_t counter = 0;
			while (!is_shutdown) {
				try {
					callback();
					break;
				} catch(...) {
					++counter;
					if (callback_error != nullptr) callback_error(counter);
					std::this_thread::sleep_for(std::chrono::milliseconds(delay));
					continue;
				}
			}
		}

		inline void create_protected_transaction(
				const std::function<void()> &callback,
				const std::function<void(const uint64_t counter)> &callback_error,
				const uint64_t delay = 500) {
			uint64_t counter = 0;
			while (!is_shutdown) {
				try {
					std::lock_guard<std::mutex> lock(backup_mutex);
					storage.transaction([&, callback]() mutable {
						callback();
						return true;
					});
					break;
				} catch(...) {
					++counter;
					if (callback_error != nullptr) callback_error(counter);
					std::this_thread::sleep_for(std::chrono::milliseconds(delay));
					continue;
				}
			}
		}
		*/

	}; // utility
}; // trading_d

#endif // PROTECTION_HPP_INCLUDED
