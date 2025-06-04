#include "Session.hpp"
#include "Connector.hpp"

Session::Session(shared_ptr<ip::tcp::socket> socket, DatabaseHandler& db_handler)
    : socket(move(socket)), db_handler_(db_handler) {
}

void Session::start() {
    do_read();
}

void Session::do_read() {
    auto self(shared_from_this());
    buffer_.clear();
    // Читаем до символа разделителя ('\0')
    async_read_until(*socket, dynamic_string_buffer(buffer_), '\0',
        [this, self](boost::system::error_code ec, size_t length) {
            if (!ec) {
                try {
                    buffer_.resize(length - 1);             
                    // Парсим JSON
                    json msg = json::parse(buffer_);
                    process_message(msg);
                    // Продолжаем чтение
                    do_read();
                }
                catch (const exception& e) {
                    cerr << "Ошибка парсинга JSON: " << e.what() << endl;
                    if (auto conn = connector_.lock()) conn->remove_session(self);
                }
            }
            else {
                if (auto conn = connector_.lock()) conn->remove_session(self);
            }
        });
}

// Отправка ответов клиенту
void Session::send_response(const json& response) {
    auto self(shared_from_this());
    auto response_str = make_shared<string>(response.dump() + '\0'); // Добавляем разделитель

    // Асинхронная отправка
    async_write(*socket, buffer(*response_str),
        [this, self, response_str](boost::system::error_code ec, size_t) {
            if (ec) {
                cerr << "Ошибка отправки ответа: " << ec.message() << endl;
                if (auto conn = connector_.lock()) {
                    conn->remove_session(self);
                }
            }
        });
}

// Обработка сообщений
void Session::process_message(const json& msg) {
    try {
        json response;
        string type = msg["type"];
        cout << "Получен запрос типа: " << type << endl;

        if (type == "auth") {
            response = handle_auth(msg);   // Авторизация
        }
        else if (type == "register") {
            string message = db_handler_.register_user(msg["username"], msg["password_hash"]);
            response = {
                {"type", "register_response"},
                { "message", message}
            };
        }
        else if (type == "create_group") {
            string group_name = msg["group_name"];
            if (db_handler_.create_group(group_name, username_)) {
                response = { {"type", "group_created"}, {"group_name", group_name} };
            }
        }
        else if (type == "invite_to_group") {       // Приглашение в группу
            string group_name = msg["group_name"];
            string user_to_add = msg["user"];
            if (db_handler_.add_user_to_group(user_to_add, group_name)) {
                response = { {"type", "user_added"}, {"group_name", group_name} };
            }
        }
        else if (type == "get_chat_messages") {
            string chat_id = msg["chat_id"];
            bool is_group = msg["is_group"];
            json messages = db_handler_.get_chat_messages(username_, chat_id, is_group);
            response = {
                {"type", "chat_messages"},
                {"chat_id", chat_id},
                {"is_group", is_group},
                {"messages", messages}
            };
        }

        send_response(response);

        if (type == "message") {
            handle_message(msg, false);        // Личное сообщение
        }
        else if (type == "group_message") {     // Групповое сообщение
            handle_message(msg, true);
        }
        else {
            response = {
                {"type", "error"},
                {"message", "Unknown message type"}
            };
            send_response(response);
        }
        
    }
    catch (const exception& e) {
        cerr << "Ошибка при обработке сообщения: " << e.what() << endl;
    }
}

// Обработка авторизации
json Session::handle_auth(const json& msg) {
    json response;
    // Проверяем наличие обязательных полей
    if (!msg.contains("username") || !msg.contains("password_hash")) {
        response = {
            {"type", "auth_response"},
            {"status", "failure"},
            {"message", "username and password are not specified"}
        };
        return response;
    }

    // Получаем данные из JSON
    string username = msg["username"].get<string>();
    string password_hash = msg["password_hash"].get<string>();

    // Валидация данных
    if (username.empty() || password_hash.empty()) {
        response = {
            {"type", "auth_response"},
            {"status", "failure"},
            {"message", "username and password cannot be empty"}
        };
        return response;
    }

    // Аутентификация пользователя
    bool auth_result = db_handler_.authenticate_user(username, password_hash);

    // Формируем ответ
    if (auth_result) {
        username_ = username; // Сохраняем имя пользователя в сессии
        response = {
            {"type", "auth_response"},
            {"status", "success"},
            {"username", username_},
            {"message", "authorization is successful"}
        };
        cout << "Успешная авторизация: " << username_ << endl;
    }
    else {
        response = {
            {"type", "auth_response"},
            {"status", "failure"},
            {"message", "invalid username or password"}
        };
        cerr << "Ошибка авторизации для: " << username << endl;
    }
    return response;
}

// Обработка сообщения
void Session::handle_message(const json& msg, bool is_group) {
    string to = msg["to"];
    string content = msg["content"];

    // Сохраняем сообщение в БД
    db_handler_.save_message(username_, to, content, is_group);

    // Пересылаем сообщение всем клиентам
    if (auto conn = connector_.lock()) {
        conn->broadcast_message(username_, to, content, is_group);
    }
}