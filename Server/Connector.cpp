#include "Connector.hpp"

Connector::Connector(io_context& io_context,
    unsigned int port,
    DatabaseHandler& db_handler)
    : io_context_(io_context),  // ������������� ��������� �����-������
    acceptor_(io_context, ip::tcp::endpoint(ip::tcp::v4(), port)),  // ������������� ���������
    db_handler_(db_handler) {   // ������������� ����������� ��
    start_accept();             // ������ �������� �����������
}

void Connector::start_accept() {
    // �������� ������ ������ ��� �������� �����������
    auto socket = make_shared<ip::tcp::socket>(io_context_);

    // ����������� �������� ������ �����������
    acceptor_.async_accept(*socket,
        [this, socket](const boost::system::error_code& error) {
            handle_accept(socket, error);   // ���������� �����������
        });
}

void Connector::handle_accept(shared_ptr<ip::tcp::socket> socket,
    const boost::system::error_code& error) {
    if (!error) {
        // ����������� ������ �����������
        cout << "����� ����������� ��: "
            << socket->remote_endpoint().address().to_string()
            << endl;

        // �������� ������ ��� ������ �������
        auto session = make_shared<Session>(socket, db_handler_);
        add_session(session);
        session->start();
    }
    else {
        cerr << "������ �����������: " << error.message() << endl;
    }
    
    // �������� ����� �����������
    start_accept();
}

void Connector::add_session(shared_ptr<Session> session) {
    lock_guard<mutex> lock(sessions_mutex_);    // ���������� ��� ������������������
    sessions_.insert(session);                  // ���������� ������ � ���������
}

void Connector::remove_session(shared_ptr<Session> session) {
    lock_guard<mutex> lock(sessions_mutex_);
    sessions_.erase(session);                   // �������� ������ �� ���������
}

void Connector::broadcast_message(const string& from, const string& target, const string& content, bool is_group){
    lock_guard<mutex> lock(sessions_mutex_);

    // �������� JSON-���������
    json message = {
        {"type", is_group ? "group_message" : "message"},
        {"from", from},                 
        {"to", target},
        {"content", content},          
        {"timestamp", time(nullptr)}    
    };

    // ����������� �����������
    if (is_group)
    {
        // ��� ���������� ��������� �������� ������ ���������� ������
        auto members = db_handler_.get_group_members(target);

        // �������� ������ ���������� ������
        for (const auto& session : sessions_) {
            // ���������, �������� �� ������������ ���������� ������
            if (find(members.begin(), members.end(), session->get_username()) != members.end()) {
                session->send_response(message);
            }
        }
    }
    else
    {
        // ��� ������� ���������
        for (const auto& session : sessions_) {
            if (session->get_username() == target) {
                session->send_response(message);
                break;
            }
        }
    }
}