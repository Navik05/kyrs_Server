#pragma once
#include <mysql.h>
#include <string>
#include <mutex>
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;
using namespace std;

class DatabaseHandler {
public:
    DatabaseHandler(const string& host,
        const string& user,
        const string& password,
        const string& database,
        unsigned int port = 3306);
    ~DatabaseHandler();
    bool connect();
    // Методы работы с пользователями
    bool authenticate_user(const string& username, const string& password_hash);
    string register_user(const string& username, const string& password_hash);
    void save_message(const string& from, const string& to,
        const string& content, bool is_group);
    json get_chat_messages(const string& username, const string& chat_id, bool is_group);
    bool create_group(const string& group_name, const string& creator_username);
    bool add_user_to_group(const string& username, const string& group_name);
    json get_group_members(const string& group_name);
    json get_user_groups(const string& username);

private:
    MYSQL* connection_;
    mutex db_mutex_;
    string host_;
    string user_;
    string password_;
    string database_;
    unsigned int port_;
    bool execute_query(const string& query);
    bool initialize_db();
};