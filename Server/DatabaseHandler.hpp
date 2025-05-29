#pragma once
#include "sqlite3.h"
#include <string>
#include <mutex>
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;
using namespace std;

class DatabaseHandler {
public:
    DatabaseHandler(const string& db_path);
    ~DatabaseHandler();

    bool authenticate_user(const string& username, const string& password_hash);
    //bool register_user(const string& username, const string& password_hash);
    //void save_message(const string& from, const string& to,
        //const string& content, bool is_group);
    //json get_message_history(const string& user1, const string& user2);

private:
    sqlite3* db_;
    mutex db_mutex_;

    void initialize_db();
    //static int callback(void* data, int argc, char** argv, char** azColName);
};