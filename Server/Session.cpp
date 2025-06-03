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
    // ������ �� ������� ����������� ('\0')
    async_read_until(*socket, dynamic_string_buffer(buffer_), '\0',
        [this, self](boost::system::error_code ec, size_t length) {
            if (!ec) {
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
                if (auto conn = connector_.lock()) conn->remove_session(self);
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
        string type = msg["type"];
        cout << "������� ������ ����: " << type << endl; // �����������

        if (type == "auth") {
            handle_auth(msg);   // �����������
        }
        else if (type == "message") {
            handle_message(msg);        // ������ ���������
        }
        else if (type == "create_group") {
            string group_name = msg["group_name"];
            if (db_handler_.create_group(group_name, username_)) {
                json response = { {"type", "group_created"}, {"group_name", group_name} };
                send_response(response);
            }
        }
        else if (type == "invite_to_group") {       // ����������� � ������
            string group_name = msg["group_name"];
            string user_to_add = msg["user"];
            if (db_handler_.add_user_to_group(user_to_add, group_name)) {
                json response = { {"type", "user_added"}, {"group_name", group_name} };
                send_response(response);
            }
        }
        else if (type == "group_message") {     // ��������� ���������
            string group_name = msg["to"];  // to � ��� ��� ������
            string content = msg["content"];
            db_handler_.save_message(username_, group_name, content, true);  // is_group = true
            if (auto conn = connector_.lock()) {
                conn->broadcast_group_message(username_, group_name, content);
            }
        }
        else if (type == "register") {
            cout << "����������� ���: " << msg["username"] << endl;
            string message =  db_handler_.register_user(msg["username"], msg["password_hash"]);
            json response = {
                {"type", "register_response"},
                { "message", message}
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
        cerr << "������ ��� ��������� ���������: " << e.what() << endl;
    }
}

// ��������� �����������
void Session::handle_auth(const json& msg) {
    // ��������� ������� ������������ �����
    if (!msg.contains("username") || !msg.contains("password_hash")) {
        json response = {
            {"type", "auth_response"},
            {"status", "failure"},
            {"message", "username and password are not specified"}
        };
        send_response(response);
        return;
    }

    // �������� ������ �� JSON
    string username = msg["username"].get<string>();
    string password_hash = msg["password_hash"].get<string>();

    // ��������� ������
    if (username.empty() || password_hash.empty()) {
        json response = {
            {"type", "auth_response"},
            {"status", "failure"},
            {"message", "username and password cannot be empty"}
        };
        send_response(response);
        return;
    }

    cout << "������� �����������: " << username << endl;

    // �������������� ������������
    bool auth_result = db_handler_.authenticate_user(username, password_hash);

    // ��������� �����
    json response;
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

    send_response(response);
}

// ��������� ���������
void Session::handle_message(const json& msg) {
    string to = msg["to"];
    string content = msg["content"];

    // ��������� ��������� � ��
    db_handler_.save_message(username_, to, content, false);

    // ���������� ��������� ���� ��������
    if (auto conn = connector_.lock()) {
        conn->broadcast_message(username_, to, content);
    }
}