#include "DatabaseHandler.hpp"
#include <stdexcept>

DatabaseHandler::DatabaseHandler(const string& host,
    const string& user,
    const string& password,
    const string& database,
    unsigned int port)
    : host_(host), user_(user), password_(password),
    database_(database), port_(port) {
    connection_ = mysql_init(nullptr);  // Инициализация соединения
    if (!connection_) {
        throw runtime_error("Не удалось выполнить инициализацию MySQL");
    }

    if (!connect()) {
        throw runtime_error("Не удалось подключиться к базе данных MySQL");
    }

    if (!initialize_db()) {
        throw runtime_error("Не удалось инициализировать базу данных");
    }
}

DatabaseHandler::~DatabaseHandler() {
    if (connection_) {  
        mysql_close(connection_);   // Закрытие соединения при уничтожении объекта
    }
}

bool DatabaseHandler::connect() {
    lock_guard<mutex> lock(db_mutex_);
    if (!mysql_real_connect(connection_, host_.c_str(), user_.c_str(),
        password_.c_str(), database_.c_str(),
        port_, nullptr, 0)) {
        cerr << "Ошибка подключения к MySQL: " << mysql_error(connection_) << endl;
        return false;
    }
    return true;
}

bool DatabaseHandler::initialize_db() {
    const vector<string> queries = {
        // Создание таблицы пользователей
        "CREATE TABLE IF NOT EXISTS users ("
        "id INT AUTO_INCREMENT PRIMARY KEY, "
        "username VARCHAR(255) UNIQUE NOT NULL, "
        "password_hash VARCHAR(255) NOT NULL, "
        "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP)",

        // Создание таблицы сообщений
        "CREATE TABLE IF NOT EXISTS messages ("
        "id INT AUTO_INCREMENT PRIMARY KEY, "
        "sender_id INT NOT NULL, "
        "receiver_id INT, "
        "group_id INT, "
        "content TEXT NOT NULL, "
        "timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
        "FOREIGN KEY (sender_id) REFERENCES users(id), "
        "FOREIGN KEY (receiver_id) REFERENCES users(id))",

        // Создание таблицы групп
        "CREATE TABLE IF NOT EXISTS team ("
        "id INT AUTO_INCREMENT PRIMARY KEY, "
        "name VARCHAR(255) NOT NULL, "
        "created_by INT NOT NULL, "
        "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
        "FOREIGN KEY (created_by) REFERENCES users(id))",

        // Создание таблицы членов группы
        "CREATE TABLE IF NOT EXISTS team_members ("
        "team_id INT NOT NULL, "
        "user_id INT NOT NULL, "
        "joined_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
        "PRIMARY KEY (team_id, user_id), "
        "FOREIGN KEY (team_id) REFERENCES team(id), "
        "FOREIGN KEY (user_id) REFERENCES users(id))"
    };

    for (const auto& query : queries) {
        // Выполнение каждого запроса
        if (!execute_query(query)) {    
            return false;
        }
    }

    return true;
}

// Базовый метод выполнения запросов
bool DatabaseHandler::execute_query(const string& query) {
    lock_guard<mutex> lock(db_mutex_);
    if (mysql_query(connection_, query.c_str()) != 0) {
        cerr << "MySQL query error: " << mysql_error(connection_) << endl;
        return false;
    }
    return true;
}

// Проверка авторизации
bool DatabaseHandler::authenticate_user(const string& username, const string& password_hash) {
    lock_guard<mutex> lock(db_mutex_);
    string query = "SELECT password_hash FROM users WHERE username = '" +
        username + "'";

    if (mysql_query(connection_, query.c_str()) != 0) {
        cerr << "Ошибка запроса MySQL: " << mysql_error(connection_) << endl;
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
        cerr << "Ошибка запроса MySQL: " << mysql_error(connection_) << endl;
    }
}

bool DatabaseHandler::create_group(const string& group_name, const string& creator_username) {
    lock_guard<mutex> lock(db_mutex_);
    string query = "INSERT INTO groups (name, created_by) "
        "VALUES ('" + group_name + "', "
        "(SELECT id FROM users WHERE username = '" + creator_username + "'))";
    return execute_query(query);
}

bool DatabaseHandler::add_user_to_group(const string& username, const string& group_name) {
    lock_guard<mutex> lock(db_mutex_);
    string query = "INSERT INTO group_members (group_id, user_id) "
        "VALUES ((SELECT id FROM groups WHERE name = '" + group_name + "'), "
        "(SELECT id FROM users WHERE username = '" + username + "'))";
    return execute_query(query);
}

json DatabaseHandler::get_group_members(const string& group_name) {
    lock_guard<mutex> lock(db_mutex_);
    json members = json::array(); // Возвращаем массив имен пользователей

    // Запрос для получения всех участников группы
    string query =
        "SELECT u.username FROM group_members gm "
        "JOIN users u ON gm.user_id = u.id "
        "JOIN groups g ON gm.group_id = g.id "
        "WHERE g.name = '" + group_name + "'";

    if (mysql_query(connection_, query.c_str()) != 0) {
        cerr << "Ошибка запроса MySQL: " << mysql_error(connection_) << endl;
        return members;
    }

    MYSQL_RES* result = mysql_store_result(connection_);
    if (!result) {
        return members;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        if (row[0]) { // Проверяем, что имя пользователя не NULL
            members.push_back(row[0]);
        }
    }

    mysql_free_result(result);
    return members;
}

json DatabaseHandler::get_user_groups(const string& username) {
    lock_guard<mutex> lock(db_mutex_);
    json groups = json::array(); // Возвращаем массив групп

    // Запрос для получения всех групп пользователя
    string query =
        "SELECT g.id, g.name, g.created_at FROM groups g "
        "JOIN group_members gm ON g.id = gm.group_id "
        "JOIN users u ON gm.user_id = u.id "
        "WHERE u.username = '" + username + "'";

    if (mysql_query(connection_, query.c_str()) != 0) {
        cerr << "Ошибка запроса MySQL (get_user_groups): " << mysql_error(connection_) << endl;
        return groups; // Пустой массив при ошибке
    }

    MYSQL_RES* result = mysql_store_result(connection_);
    if (!result) {
        return groups;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        json group_info = {
            {"group_id", row[0]},
            {"group_name", row[1]},
            {"created_at", row[2]}
        };
        groups.push_back(group_info);
    }

    mysql_free_result(result);
    return groups;
}

// Регистрация пользователя
string DatabaseHandler::register_user(const string& username, const string& password_hash) {
    lock_guard<mutex> lock(db_mutex_);

    string message;
    // Проверка существования пользователя
    string check_query = "SELECT id FROM users WHERE username = '" + username + "'";
    if (mysql_query(connection_, check_query.c_str()) ){
        message = "User verification error";
        cerr << "Ошибка проверки пользователя: " << mysql_error(connection_) << endl;
        return message;
    }

    MYSQL_RES* result = mysql_store_result(connection_);
        if (result && mysql_num_rows(result) > 0) {
            message = "The user already exists";
            cerr << "Пользователь уже существует" << endl;
            mysql_free_result(result);
            return message;
        }
    if (result) mysql_free_result(result);

    // Регистрация нового пользователя
    string insert_query = "INSERT INTO users (username, password_hash) VALUES ('" +
        username + "', '" + password_hash + "')";

    if (mysql_query(connection_, insert_query.c_str())) {
        message = "Registration error";
        cerr << "Ошибка регистрации: " << mysql_error(connection_) << endl;
        return message;
    }
    mysql_affected_rows(connection_) == 1;
}

// Передача истории сообщений в чат
json DatabaseHandler::get_chat_messages(const string& username, const string& chat_id, bool is_group) {
    lock_guard<mutex> lock(db_mutex_);
    json messages = json::array();

    string query;
    if (is_group) {
        query =
            "SELECT u.username as sender, m.content, m.timestamp "
            "FROM messages m "
            "JOIN users u ON m.sender_id = u.id "
            "WHERE m.group_id = (SELECT id FROM team WHERE name = '" + chat_id + "') "
            "ORDER BY m.timestamp";
    }
    else {
        query =
        "SELECT u.username as sender, m.content, m.timestamp "
            "FROM messages m "
            "JOIN users u ON m.sender_id = u.id "
            "WHERE (m.sender_id = (SELECT id FROM users WHERE username = '" + username + "') "
            "AND m.receiver_id = (SELECT id FROM users WHERE username = '" + chat_id + "')) "
            "OR (m.sender_id = (SELECT id FROM users WHERE username = '" + chat_id + "') "
            "AND m.receiver_id = (SELECT id FROM users WHERE username = '" + username + "')) "
            "ORDER BY m.timestamp";
    }

    if (mysql_query(connection_, query.c_str())) {
        cerr << "Ошибка запроса истории чата: " << mysql_error(connection_) << endl;
        return messages;
    }

    MYSQL_RES* result = mysql_store_result(connection_);
    if (!result) return messages;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        json message = {
            {"from", row[0]},
            {"content", row[1]},
            {"timestamp", row[2]}
        };
        messages.push_back(message);
    }

    mysql_free_result(result);
    return messages;
};