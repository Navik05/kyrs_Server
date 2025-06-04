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
    MYSQL* connection_;
    DatabaseHandler(const string& host,
        const string& user,
        const string& password,
        const string& database,
        unsigned int port = 3306);
    ~DatabaseHandler();
    bool connect();
    bool authenticate_user(const string& username, const string& password_hash);
    string register_user(const string& username, const string& password_hash);
    void save_message(const string& from, const string& to,
        const string& content, bool is_team);
    json get_chat_messages(const string& username, const string& chat_id, bool is_team);
    bool create_team(const string& team_name, const string& creator_username);
    bool add_user_to_team(const string& username, const string& team_name);
    json get_team_members(const string& team_name);
    json get_user_team(const string& username);
    bool execute_query(const string& query);

private:
    mutex db_mutex_;
    string host_;
    string user_;
    string password_;
    string database_;
    unsigned int port_;
    bool initialize_db();
};