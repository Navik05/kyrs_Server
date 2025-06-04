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
    // ������ �� ������� ����������� '\0'
    async_read_until(*socket, dynamic_string_buffer(buffer_), '\0',
        [this, self](boost::system::error_code ec, size_t length) {
            if (!ec && length > 0) {
                try {
                    buffer_.resize(length - 1);             
                    // ������ JSON
                    json msg = json::parse(buffer_);
                    process_message(msg);
                    // ���������� ������
                    do_read();
                }
                catch (const exception& e) {
                    cerr << "������ �������� JSON: " << e.what() << endl;
                    if (auto conn = connector_.lock()) conn->remove_session(self);
                }
            }
            else {
                if (ec != error::operation_aborted) {
                    cerr << "������ ������: " << ec.message() << endl;
                    if (auto conn = connector_.lock()) conn->remove_session(self);
                }
            }
        });
}

// �������� ������� �������
void Session::send_response(const json& response) {
    auto self(shared_from_this());
    auto response_str = make_shared<string>(response.dump() + '\0'); // ��������� �����������

    // ����������� ��������
    async_write(*socket, buffer(*response_str),
        [this, self, response_str](boost::system::error_code ec, size_t) {
            if (ec) {
                cerr << "������ �������� ������: " << ec.message() << endl;
                if (auto conn = connector_.lock()) {
                    conn->remove_session(self);
                }
            }
        });
}

// ��������� ���������
void Session::process_message(const json& msg) {
    try {
        json response;
        string type = msg["type"];
        cout << "������� ������ ����: " << type << endl;
        // �����������
        if (type == "auth") {
            response = handle_auth(msg);
        }
        // �����������
        else if (type == "register") {
            string message = db_handler_.register_user(msg["username"], msg["password_hash"]);
            response = {
                {"type", "register_response"},
                { "message", message}
            };
        }
        // �������� ������
        else if (type == "create_team") {
            if (db_handler_.create_team(msg["team_name"], username_)) {
                // ������������� ��������� ��������� � ������
                if (db_handler_.add_user_to_team(username_, msg["team_name"])) {
                    response = {
                        {"type", "team_created"},
                        {"team_name", msg["team_name"]}
                    };
                }
                else {
                    response = {
                        {"type", "error"},
                        {"message", "Failed to add creator to team"}
                    };
                }
            }
            else {
                response = {
                    {"type", "error"},
                    {"message", "Failed to create team"}
                };
            }
        }
        // ����������� � ������
        else if (type == "invite_to_team") {       
            if (db_handler_.add_user_to_team(msg["user"], msg["team_name"])) {
                response = { {"type", "user_added"}, {"team_name", msg["team_name"]} };
            }
        }
        // �������� ������� ����
        else if (type == "get_chat_messages") {
            json messages = db_handler_.get_chat_messages(username_, msg["chat_id"], msg["is_team"]);
            response = {
                {"type", "chat_messages"},
                {"chat_id", msg["chat_id"]},
                {"is_team", msg["is_team"]},
                {"messages", messages}
            };
        }
        // ������ �����
        else if (type == "get_chat_list") {
            response = get_available_chats();
        }
        // ������ ���������
        else if (type == "message") {
            handle_message(msg, false);
            return;
        }
        // ��������� ���������
        else if (type == "team_message") {
            handle_message(msg, true);
            return;
        }
        else {
            response = {
                {"type", "error"},
                {"message", "Unknown message type"}
            };
        }
        send_response(response);
    }
    catch (const exception& e) {
        cerr << "������ ��� ��������� ���������: " << e.what() << endl;
    }
}

// ��������� �����������
json Session::handle_auth(const json& msg) {
    json response;
    // ��������� ������� ������������ �����
    if (!msg.contains("username") || !msg.contains("password_hash")) {
        response = {
            {"type", "auth_response"},
            {"status", "failure"},
            {"message", "username and password are not specified"}
        };
        return response;
    }

    // �������� ������ �� JSON
    string username = msg["username"].get<string>();
    string password_hash = msg["password_hash"].get<string>();

    // ��������� ������
    if (username.empty() || password_hash.empty()) {
        response = {
            {"type", "auth_response"},
            {"status", "failure"},
            {"message", "username and password cannot be empty"}
        };
        return response;
    }

    // �������������� ������������
    bool auth_result = db_handler_.authenticate_user(username, password_hash);

    // ��������� �����
    if (auth_result) {
        username_ = username; // ��������� ��� ������������ � ������
        response = {
            {"type", "auth_response"},
            {"status", "success"},
            {"username", username_},
            {"message", "authorization is successful"}
        };
        cout << "�������� �����������: " << username_ << endl;
    }
    else {
        response = {
            {"type", "auth_response"},
            {"status", "failure"},
            {"message", "invalid username or password"}
        };
        cerr << "������ ����������� ���: " << username << endl;
    }
    return response;
}

// ��������� ���������
void Session::handle_message(const json& msg, bool is_team) {
    string to = msg["to"];
    string content = msg["content"];
    // ��������� ��������� � ��
    db_handler_.save_message(username_, to, content, is_team);
    // ���������� ��������� ���� ��������
    if (auto conn = connector_.lock()) {
        conn->broadcast_message(username_, to, content, is_team);
    }
    else {
     cerr << "������: �� ������� �������� Connector ��� �������� ���������" << endl;
    }
}

json Session::get_available_chats() {
    json response;

    // �������� ������ ���� �������������
    string users_query = "SELECT username FROM users";
    if (db_handler_.execute_query(users_query)) {
        MYSQL_RES* users_result = mysql_store_result(db_handler_.connection_);
        if (users_result) {
            json users = json::array();
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(users_result))) {
                users.push_back(row[0]);
            }
            response["users"] = users;
            mysql_free_result(users_result);
        }
    }

    // �������� ������ ����� �������� ������������
    json teams = db_handler_.get_user_team(username_);
    response["teams"] = teams;

    return {
        {"type", "chat_list"},
        {"data", response}
    };
}