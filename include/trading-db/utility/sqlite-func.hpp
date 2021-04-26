#pragma once
#ifndef TRADING_DB_SQLITE_FUNC_HPP_INCLUDED
#define TRADING_DB_SQLITE_FUNC_HPP_INCLUDED

#include "../config.hpp"
#include "print.hpp"
#include <sqlite3.h>
#include <string>

namespace trading_db {
    namespace utility {

        class SqliteStmt {
        private:
            sqlite3_stmt *stmt = nullptr;
        public:

            SqliteStmt() {};

            SqliteStmt(sqlite3 *sqlite_db, const char *query) {
                if(sqlite3_prepare_v2(sqlite_db, query, -1, &stmt, nullptr) != SQLITE_OK) {
                    sqlite3_finalize(stmt);
                    stmt = nullptr;
                    TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "]" << std::endl;
                }
            }

            inline void init(sqlite3 *sqlite_db, const char *query) noexcept {
                if(sqlite3_prepare_v2(sqlite_db, query, -1, &stmt, nullptr) != SQLITE_OK) {
                    sqlite3_finalize(stmt);
                    stmt = nullptr;
                    TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "]" << std::endl;
                }
            }

            ~SqliteStmt() {
                if (stmt != nullptr) {
                   sqlite3_finalize(stmt);
                   stmt = nullptr;
                }
            }

            inline sqlite3_stmt *get() noexcept {
                return stmt;
            }
        };

        class SqliteTransaction {
        private:
            SqliteStmt stmt_begin_transaction;
            SqliteStmt stmt_commit;
            SqliteStmt stmt_rollback;
            sqlite3 *db = nullptr;
        public:

            SqliteTransaction() {};

            SqliteTransaction(sqlite3 *sqlite_db) :
                stmt_begin_transaction(sqlite_db, "BEGIN TRANSACTION"),
                stmt_commit(sqlite_db, "COMMIT"),
                stmt_rollback(sqlite_db, "ROLLBACK") {
                db = sqlite_db;
            }

            inline void init(sqlite3 *sqlite_db) noexcept {
                stmt_begin_transaction.init(sqlite_db, "BEGIN TRANSACTION");
                stmt_commit.init(sqlite_db, "COMMIT");
                stmt_rollback.init(sqlite_db, "ROLLBACK");
                db = sqlite_db;
            }

            inline bool begin_transaction() noexcept {
                while (true) {
                    int err = sqlite3_step(stmt_begin_transaction.get());
                    sqlite3_reset(stmt_begin_transaction.get());
                    if(err == SQLITE_DONE) {
                        return true;
                    } else
                    if(err == SQLITE_BUSY) {
                        //...
                    } else {
                        TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: sqlite3_step return " << err << std::endl;
                        //...
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                return false;
            }

            inline bool commit() noexcept {
                while (true) {
                    int err = sqlite3_step(stmt_commit.get());
                    sqlite3_reset(stmt_commit.get());
                    if(err == SQLITE_DONE) {
                        return true;
                    } else
                    if(err == SQLITE_BUSY) {
                        // Если инструкция является COMMIT или происходит вне явной транзакции, вы можете повторить инструкцию
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        continue;
                    } else {
                        TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: sqlite3_step return code " << err << std::endl;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                return false;
            }

            bool rollback() {
                int err = sqlite3_step(stmt_rollback.get());
                sqlite3_reset(stmt_rollback.get());
                if (err == SQLITE_DONE) {
                    return true;
                } else {
                    TRADING_DB_TICK_DB_PRINT << "trading_db error in [file " << __FILE__ << ", line " << __LINE__ << ", func " << __FUNCTION__ << "], message: sqlite3_step return code " << err << std::endl;
                }
                return false;
            }
        };

        bool prepare(sqlite3 *sqlite_db, const char *query) {
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
    }; // utility
}; // trading_db

#endif // TRADING_DB_SQLITE_FUNC_HPP_INCLUDED
