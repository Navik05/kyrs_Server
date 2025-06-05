#include "Connector.hpp"

Connector::Connector(io_context& io_context,
    unsigned int port,
    DatabaseHandler& db_handler)
    : io_context_(io_context),                                      // ������������� ��������� �����-������
    acceptor_(io_context, ip::tcp::endpoint(ip::tcp::v4(), port)),  // ������������� ���������
    db_handler_(db_handler) {                                       // ������������� ����������� ��
    start_accept();                                                 // ������ �������� �����������
}

void Connector::start_accept() {
    // �������� ������ ������ ��� �������� �����������
    auto socket = make_shared<ip::tcp::socket>(io_context_);

    // ����������� �������� ������ �����������
    acceptor_.async_accept(*socket,
        [this, socket](const boost::system::error_code& error) {
            handle_accept(socket, error);
        });
}

void Connector::handle_accept(shared_ptr<ip::tcp::socket> socket, const boost::system::error_code& error) {
    if (!error) {
        cout << "����� ����������� ��: " << socket->remote_endpoint().address().to_string() << endl;

        // �������� ������ ��� ������ �������
        auto session = make_shared<Session>(socket, db_handler_);
        session->set_connector(shared_from_this());
        add_session(session);
        session->start();
    }
    else {
        cerr << "������ �����������: " << error.message() << endl;
    }

    // �������� ����� �����������
    start_accept();
}

// ���������� ������ � ���������
void Connector::add_session(shared_ptr<Session> session) {
    lock_guard<mutex> lock(sessions_mutex_);
    sessions_.insert(session);                  
}

// �������� ������ �� ���������
void Connector::remove_session(shared_ptr<Session> session) {
    lock_guard<mutex> lock(sessions_mutex_);
    sessions_.erase(session);                   
}

void Connector::broadcast_message(const string& from, const string& target, const string& content, bool is_team){
    // �������� JSON-���������
    lock_guard<mutex> lock(sessions_mutex_);
    json message = {
        {"type", is_team ? "team_message" : "message"},
        {"from", from},                 
        {"to", target},
        {"content", content},          
        {"timestamp", time(nullptr)}    
    };

    // ����������� �����������
    unordered_set<string> recipients;
    if (is_team){
        auto members = db_handler_.get_team_members(target);
        recipients.insert(members.begin(), members.end());
    }
    else {
        recipients.insert(from);
        recipients.insert(target);
    }

    // ��������� ���� ���������� �������
    for (const auto& session : sessions_) {
        if (!session) continue;

        string username = session->get_username();
        if (recipients.count(username) > 0) {
            try {
                session->send_response(message);
            }
            catch (const exception& e) {
                cerr << "������ �������� ��� " << username << ": " << e.what() << endl;
            }
        }
    }
}