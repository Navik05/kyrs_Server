#include "DatabaseHandler.hpp"

DatabaseHandler::DatabaseHandler(const string& db_path) {
    if (sqlite3_open(db_path.c_str(), &db_) != SQLITE_OK) {
        cerr << "Не удалось открыть базу данных: " << sqlite3_errmsg(db_) << endl;
        exit(1);
    }
    initialize_db();
}

DatabaseHandler::~DatabaseHandler() {
    sqlite3_close(db_);
}

void DatabaseHandler::initialize_db() {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            password_hash TEXT NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );
        
        CREATE TABLE IF NOT EXISTS messages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            sender_id INTEGER NOT NULL,
            receiver_id INTEGER,
            group_id INTEGER,
            content TEXT NOT NULL,
            timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY(sender_id) REFERENCES users(id),
            FOREIGN KEY(receiver_id) REFERENCES users(id)
        );
    )";

    char* err_msg = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &err_msg) != SQLITE_OK) {
        cerr << "SQL ошибка: " << err_msg << endl;
        sqlite3_free(err_msg);
    }
}

bool DatabaseHandler::authenticate_user(const string& username, const string& password_hash) {
    lock_guard<mutex> lock(db_mutex_);
    string sql = "SELECT password_hash FROM users WHERE username = ?";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);

    bool result = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        string stored_hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        result = (stored_hash == password_hash);
    }

    sqlite3_finalize(stmt);
    return result;
}