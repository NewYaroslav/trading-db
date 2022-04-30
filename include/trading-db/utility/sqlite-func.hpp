#pragma once
#ifndef TRADING_DB_SQLITE_FUNC_HPP_INCLUDED
#define TRADING_DB_SQLITE_FUNC_HPP_INCLUDED

#include "../config.hpp"
#include "print.hpp"
#include <sqlite3.h>
#include <string>
#include <functional>

namespace trading_db {
	namespace utility {

		/** \brief Класс для предкомпиляции запроса SQL
		 */
		class SqliteStmt {
		private:
			sqlite3_stmt *stmt = nullptr;
			int err = SQLITE_OK;
		public:

			std::function<void(const int code, const std::string message)> on_error = nullptr;

			SqliteStmt() {};

			SqliteStmt(sqlite3 *sqlite_db, const char *query) {
				err = sqlite3_prepare_v2(sqlite_db, query, -1, &stmt, nullptr);
				if(err != SQLITE_OK) {
					sqlite3_finalize(stmt);
					stmt = nullptr;
				}
			}


			~SqliteStmt() {
				if (stmt) {
				   sqlite3_finalize(stmt);
				   stmt = nullptr;
				}
			}


			inline bool init(sqlite3 *sqlite_db, const std::string &request) noexcept {
				const char *query = request.c_str();
				err = sqlite3_prepare_v2(sqlite_db, query, -1, &stmt, nullptr);
				if(err != SQLITE_OK) {
					sqlite3_finalize(stmt);
					stmt = nullptr;
					if (on_error != nullptr) on_error(err, "sqlite3_prepare_v2 error");
					return false;
				}
				return true;
			}

			inline const int get_error_code() noexcept {
				return err;
			}

			inline sqlite3_stmt *get() noexcept {
				return stmt;
			}
		};

		/** \brief Класс для осуществления транзакций
		 */
		class SqliteTransaction {
		private:
			SqliteStmt stmt_begin_transaction;
			SqliteStmt stmt_commit;
			SqliteStmt stmt_rollback;
			sqlite3 *db = nullptr;
		public:

			std::function<void(const int code, const std::string &message)> on_error = nullptr;

			SqliteTransaction() {};

			SqliteTransaction(sqlite3 *sqlite_db) :
				stmt_begin_transaction(sqlite_db, "BEGIN TRANSACTION"),
				stmt_commit(sqlite_db, "COMMIT"),
				stmt_rollback(sqlite_db, "ROLLBACK") {
				db = sqlite_db;
			}

			inline bool init(sqlite3 *sqlite_db) noexcept {
				stmt_begin_transaction.init(sqlite_db, "BEGIN TRANSACTION");
				stmt_commit.init(sqlite_db, "COMMIT");
				stmt_rollback.init(sqlite_db, "ROLLBACK");
				db = sqlite_db;
				if (stmt_begin_transaction.get_error_code() != SQLITE_OK) return false;
				if (stmt_commit.get_error_code() != SQLITE_OK) return false;
				if (stmt_rollback.get_error_code() != SQLITE_OK) return false;
				return true;
			}

			inline bool begin_transaction() noexcept {
				while (true) {
					const int err = sqlite3_step(stmt_begin_transaction.get());
					sqlite3_reset(stmt_begin_transaction.get());
					if(err == SQLITE_DONE) {
						return true;
					} else
					if(err == SQLITE_BUSY) {
						//...
					} else {
						if (on_error != nullptr) on_error(err, "sqlite3_step error");
					}
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
				}
				return false;
			}

			inline bool commit() noexcept {
				while (true) {
					const int err = sqlite3_step(stmt_commit.get());
					sqlite3_reset(stmt_commit.get());
					if(err == SQLITE_DONE) {
						return true;
					} else
					if(err == SQLITE_BUSY) {
						// Если инструкция является COMMIT или происходит вне явной транзакции, вы можете повторить инструкцию
						std::this_thread::sleep_for(std::chrono::milliseconds(1));
						continue;
					} else {
						if (on_error != nullptr) on_error(err, "sqlite3_step error");
					}
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
				}
				return false;
			}

			bool rollback() {
				const int err = sqlite3_step(stmt_rollback.get());
				sqlite3_reset(stmt_rollback.get());
				if (err == SQLITE_DONE) {
					return true;
				} else {
					if (on_error) on_error(err, "sqlite3_step error");
				}
				return false;
			}
		};

		bool prepare(sqlite3 *sqlite_db, const std::string &request) {
			const char *query = request.c_str();
			sqlite3_stmt *stmt = nullptr;
			if(sqlite3_prepare_v2(sqlite_db, query, -1, &stmt, nullptr) == SQLITE_OK) {
				int err = sqlite3_step(stmt);
				sqlite3_reset(stmt);
				sqlite3_finalize(stmt);
				if(err == SQLITE_DONE) {
					//...
				} else {
					TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: " << sqlite3_errmsg(sqlite_db) << std::endl;
					return false;
				}
			} else {
				TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: " << sqlite3_errmsg(sqlite_db) << std::endl;
				return false;
			}
			return true;
		}

		bool sqlite_exec(sqlite3 *sqlite_db_ptr, const std::string &sql_statement) {
			char *err = nullptr;
			if(sqlite3_exec(sqlite_db_ptr, sql_statement.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
				TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: " << err << std::endl;
				sqlite3_free(err);
				return false;
			}
			return true;
		}

		bool backup_form_db(const std::string &path, sqlite3 *source_connection) {
			sqlite3 *dest_connection = nullptr;

			int flags = (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX);
			if (sqlite3_open_v2(path.c_str(), &dest_connection, flags, nullptr) != SQLITE_OK) {
				TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: " << sqlite3_errmsg(dest_connection) << ", db name " << path << std::endl;
				sqlite3_close_v2(dest_connection);
				dest_connection = nullptr;
				return false;
			}

			sqlite3_backup *backup_db = sqlite3_backup_init(dest_connection, "main", source_connection, "main");
			if (!backup_db) {
				sqlite3_close_v2(dest_connection);
				return false;
			}

			while (true) {
				int err = sqlite3_backup_step(backup_db, -1);
				bool is_break = false;
				switch(err) {
				case SQLITE_DONE:
					//continue;
					is_break = true;
					break;
				case SQLITE_OK:
				case SQLITE_BUSY:
				case SQLITE_LOCKED:
					//TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: sqlite3_backup_step return code " << err << std::endl;
					break;
				default:
					TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: sqlite3_backup_step return code " << err << std::endl;
					is_break = true;
					break;
				}
				if (is_break) break;
				std::this_thread::sleep_for(std::chrono::milliseconds(250));
			}

			if (sqlite3_backup_finish(backup_db) != SQLITE_OK) {
				sqlite3_close_v2(dest_connection);
				return false;
			}
			sqlite3_close_v2(dest_connection);
			return true;
		}
	}; // utility
}; // trading_db

#endif // TRADING_DB_SQLITE_FUNC_HPP_INCLUDED
