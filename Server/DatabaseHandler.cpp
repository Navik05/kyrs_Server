#include "DatabaseHandler.hpp"
#include <stdexcept>

DatabaseHandler::DatabaseHandler(const string& host,
    const string& user,
    const string& password,
    const string& database,
    unsigned int port)
    : host_(host), user_(user), password_(password),
    database_(database), port_(port) {
    connection_ = mysql_init(nullptr);
    if (!connection_) {
        throw runtime_error("�� ������� ��������� ������������� MySQL");
    }

    if (!connect()) {
        throw runtime_error("�� ������� ������������ � ���� ������ MySQL");
    }

    if (!initialize_db()) {
        throw runtime_error("�� ������� ���������������� ���� ������");
    }
}

DatabaseHandler::~DatabaseHandler() {
    if (connection_) {
        mysql_close(connection_);
    }
}

bool DatabaseHandler::connect() {
    lock_guard<mutex> lock(db_mutex_);
    if (!mysql_real_connect(connection_, host_.c_str(), user_.c_str(),
        password_.c_str(), database_.c_str(),
        port_, nullptr, 0)) {
        cerr << "������ ����������� � MySQL: " << mysql_error(connection_) << endl;
        return false;
    }
    return true;
}

bool DatabaseHandler::initialize_db() {
    const vector<string> queries = {
        "CREATE TABLE IF NOT EXISTS users ("
        "id INT AUTO_INCREMENT PRIMARY KEY, "
        "username VARCHAR(255) UNIQUE NOT NULL, "
        "password_hash VARCHAR(255) NOT NULL, "
        "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP)",

        "CREATE TABLE IF NOT EXISTS messages ("
        "id INT AUTO_INCREMENT PRIMARY KEY, "
        "sender_id INT NOT NULL, "
        "receiver_id INT, "
        "group_id INT, "
        "content TEXT NOT NULL, "
        "timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
        "FOREIGN KEY (sender_id) REFERENCES users(id), "
        "FOREIGN KEY (receiver_id) REFERENCES users(id))"
    };

    for (const auto& query : queries) {
        if (!execute_query(query)) {
            return false;
        }
    }
    return true;
}

bool DatabaseHandler::execute_query(const string& query) {
    lock_guard<mutex> lock(db_mutex_);
    if (mysql_query(connection_, query.c_str()) != 0) {
        cerr << "MySQL query error: " << mysql_error(connection_) << endl;
        return false;
    }
    return true;
}

bool DatabaseHandler::authenticate_user(const string& username, const string& password_hash) {
    lock_guard<mutex> lock(db_mutex_);
    string query = "SELECT password_hash FROM users WHERE username = '" +
        username + "'";

    if (mysql_query(connection_, query.c_str()) != 0) {
        cerr << "������ ������� MySQL: " << mysql_error(connection_) << endl;
        return false;
    }

    MYSQL_RES* result = mysql_store_result(connection_);
    if (!result) {
        return false;
    }

    bool auth_success = false;
    MYSQL_ROW row = mysql_fetch_row(result);
    if (row) {
        string stored_hash = row[0];
        auth_success = (stored_hash == password_hash);
    }

    mysql_free_result(result);
    return auth_success;
}

bool DatabaseHandler::register_user(const string& username, const string& password_hash) {
    lock_guard<mutex> lock(db_mutex_);
    string query = "INSERT INTO users (username, password_hash) VALUES ('" +
        username + "', '" + password_hash + "')";

    if (mysql_query(connection_, query.c_str()) != 0) {
        cerr << "������ ������� MySQL: " << mysql_error(connection_) << endl;
        return false;
    }
    return mysql_affected_rows(connection_) == 1;
}

void DatabaseHandler::save_message(const string& from, const string& to,
    const string& content, bool is_group) {
    lock_guard<mutex> lock(db_mutex_);
    string query;

    if (is_group) {
        query = "INSERT INTO messages (sender_id, group_id, content) "
            "VALUES ((SELECT id FROM users WHERE username = '" + from + "'), "
            "'" + to + "', '" + content + "')";
    }
    else {
        query = "INSERT INTO messages (sender_id, receiver_id, content) "
            "VALUES ((SELECT id FROM users WHERE username = '" + from + "'), "
            "(SELECT id FROM users WHERE username = '" + to + "'), "
            "'" + content + "')";
    }

    if (mysql_query(connection_, query.c_str()) != 0) {
        cerr << "������ ������� MySQL: " << mysql_error(connection_) << endl;
    }
}

json DatabaseHandler::get_message_history(const string& user1, const string& user2) {
    lock_guard<mutex> lock(db_mutex_);
    json result = json::array();

    string query = "SELECT u1.username as sender, u2.username as receiver, "
        "m.content, m.timestamp FROM messages m "
        "JOIN users u1 ON m.sender_id = u1.id "
        "LEFT JOIN users u2 ON m.receiver_id = u2.id "
        "WHERE (u1.username = '" + user1 + "' AND u2.username = '" + user2 + "') "
        "OR (u1.username = '" + user2 + "' AND u2.username = '" + user1 + "') "
        "ORDER BY m.timestamp";

    if (mysql_query(connection_, query.c_str()) != 0) {
        cerr << "������ ������� MySQL: " << mysql_error(connection_) << endl;
        return result;
    }

    MYSQL_RES* sql_result = mysql_store_result(connection_);
    if (!sql_result) {
        return result;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(sql_result))) {
        json message = {
            {"from", row[0]},
            {"to", row[1]},
            {"content", row[2]},
            {"timestamp", row[3]}
        };
        result.push_back(message);
    }

    mysql_free_result(sql_result);
    return result;
}