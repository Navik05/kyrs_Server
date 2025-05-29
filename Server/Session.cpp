#include "Session.hpp"
#include "Connector.hpp"

Session::Session(shared_ptr<ip::tcp::socket> socket, DatabaseHandler& db_handler)
    : socket(move(socket)), db_handler_(db_handler) {
}

void Session::start() {
    do_read();
}

void Session::do_read() {
    cout << "Ожидание данных от клиента..." << endl;
    auto self(shared_from_this());
    buffer_.resize(4);  // Сначала читаем 4 байта длины

    async_read(*socket, buffer(buffer_),
        [this, self](boost::system::error_code ec, size_t length) {
            if (!ec) {
                cout << "Получены байты длины: ";
                for (auto b : buffer_) cout << (int)b << " ";  // Логируем байты
                cout << endl;

                uint32_t msg_length = (static_cast<uint32_t>(buffer_[0]) << 24) |
                    (static_cast<uint32_t>(buffer_[1]) << 16) |
                    (static_cast<uint32_t>(buffer_[2]) << 8) |
                    static_cast<uint32_t>(buffer_[3]);
                cout << "Ожидаемая длина сообщения: " << msg_length << endl;
                read_body(msg_length);
            }
            else {
                cerr << "Ошибка чтения длины: " << ec.message() << endl;
                if (auto conn = connector_.lock()) conn->remove_session(self);
            }
        });
}

void Session::read_body(size_t length) {
    auto self(shared_from_this());
    buffer_.resize(length);

    async_read(*socket, buffer(buffer_),
        [this, self](boost::system::error_code ec, size_t) {
            if (!ec) {
                cout << "Получены сырые данные: ";
                for (size_t i = 0; i < buffer_.size() && i < 10; ++i) {
                    cout << (int)buffer_[i] << " ";  // Логируем первые 10 байт
                }
                cout << endl;

                try {
                    string msg_str(buffer_.begin(), buffer_.end());
                    cout << "Полученная строка: " << msg_str << endl;   // Логируем строку

                    json msg = json::parse(buffer_);
                    process_message(msg);
                }
                catch (const exception& e) {
                    cerr << "Ошибка парсинга JSON: " << e.what() << endl;
                }
                do_read();
            }
            else {
                cerr << "Ошибка чтения тела: " << ec.message() << endl;
                if (auto conn = connector_.lock()) {
                    conn->remove_session(self);
                }
            }
        });
}

void Session::process_message(const json& msg) {
    try {
        string type = msg["type"];
        cout << "Получен запрос типа: " << type << endl; // Логирование

        if (type == "auth") {
            handle_auth(msg);
        }
        else if (type == "message") {
            handle_message(msg);
        }
        else if (type == "create_group") {
            string group_name = msg["group_name"];
            if (db_handler_.create_group(group_name, username_)) {
                json response = { {"type", "group_created"}, {"group_name", group_name} };
                send_response(response);
            }
        }
        else if (type == "invite_to_group") {
            string group_name = msg["group_name"];
            string user_to_add = msg["user"];
            if (db_handler_.add_user_to_group(user_to_add, group_name)) {
                json response = { {"type", "user_added"}, {"group_name", group_name} };
                send_response(response);
            }
        }
        else if (type == "group_message") {
            string group_name = msg["to"];  // Теперь "to" — это имя группы
            string content = msg["content"];
            db_handler_.save_message(username_, group_name, content, true);  // is_group = true
            if (auto conn = connector_.lock()) {
                conn->broadcast_group_message(username_, group_name, content);
            }
        }
        else if (type == "register") {
            cout << "Регистрация для: " << msg["username"] << endl;
            string username = msg["username"];
            string password_hash = msg["password_hash"];

            bool reg_result = db_handler_.register_user(username, password_hash);

            json response = {
                {"type", "register_response"},
                {"status", reg_result ? "success" : "failure"},
                {"message", reg_result ? "Регистрация успешна" : "Ошибка регистрации"}
            };
            send_response(response);
        }
        else {
            json response = {
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

void Session::handle_auth(const json& msg) {
    try {
        // Проверяем наличие обязательных полей
        if (!msg.contains("username") || !msg.contains("password_hash")) {
            json response = {
                {"type", "auth_response"},
                {"status", "failure"},
                {"message", "Необходимы username и password_hash"}
            };
            send_response(response);
            return;
        }

        // Получаем данные из JSON
        string username = msg["username"].get<string>();
        string password_hash = msg["password_hash"].get<string>();

        // Валидация данных
        if (username.empty() || password_hash.empty()) {
            json response = {
                {"type", "auth_response"},
                {"status", "failure"},
                {"message", "Имя пользователя и пароль не могут быть пустыми"}
            };
            send_response(response);
            return;
        }

        cout << "Попытка авторизации: " << username << endl;

        // Аутентификация пользователя
        bool auth_result = db_handler_.authenticate_user(username, password_hash);

        // Формируем ответ
        json response;
        if (auth_result) {
            username_ = username; // Сохраняем имя пользователя в сессии
            response = {
                {"type", "auth_response"},
                {"status", "success"},
                {"username", username_},
                {"message", "Авторизация успешна"}
            };
            cout << "Успешная авторизация: " << username_ << endl;
        }
        else {
            response = {
                {"type", "auth_response"},
                {"status", "failure"},
                {"message", "Неверное имя пользователя или пароль"}
            };
            cerr << "Ошибка авторизации для: " << username << endl;
        }

        send_response(response);

    }
    catch (const json::exception& e) {
        cerr << "Ошибка обработки JSON при авторизации: " << e.what() << endl;
        json response = {
            {"type", "auth_response"},
            {"status", "failure"},
            {"message", "Ошибка формата запроса"}
        };
        send_response(response);
    }
    catch (const exception& e) {
        cerr << "Ошибка при обработке авторизации: " << e.what() << endl;
        json response = {
            {"type", "auth_response"},
            {"status", "failure"},
            {"message", "Внутренняя ошибка сервера"}
        };
        send_response(response);
    }
}

void Session::handle_message(const json& msg) {
    string to = msg["to"];
    string content = msg["content"];

    // Сохраняем сообщение в БД
    db_handler_.save_message(username_, to, content, false);

    // Пересылаем сообщение всем клиентам
    if (auto conn = connector_.lock()) {
        conn->broadcast_message(username_, to, content);
    }
}

void Session::send_response(const json& response) {
    auto self(shared_from_this());
    string response_str = response.dump();
    uint32_t length = response_str.size();

    vector<const_buffer> buffers;
    buffers.push_back(buffer(&length, sizeof(length)));
    buffers.push_back(buffer(response_str));

    async_write(*socket, buffers,
        [this, self](boost::system::error_code ec, size_t) {
            if (ec) {
                if (auto conn = connector_.lock()) {
                    conn->remove_session(self);
                }
            }
        });
}