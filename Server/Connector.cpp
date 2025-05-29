#include "Connector.hpp"

Connector::Connector(io_context& io_context,
    unsigned int port,
    DatabaseHandler& db_handler)
    : io_context_(io_context),
    acceptor_(io_context, ip::tcp::endpoint(ip::tcp::v4(), port)),
    db_handler_(db_handler) {
    start_accept();
}

void Connector::start_accept() {
    auto socket = make_shared<ip::tcp::socket>(io_context_);

    acceptor_.async_accept(*socket,
        [this, socket](const boost::system::error_code& error) {
            handle_accept(socket, error);
        });
}

void Connector::handle_accept(shared_ptr<ip::tcp::socket> socket,
    const boost::system::error_code& error) {
    if (!error) {
        cout << "Новое подключение от: "
            << socket->remote_endpoint().address().to_string()
            << endl; // Логируем IP клиента

        auto session = make_shared<Session>(socket, db_handler_);
        add_session(session);
        session->start();
    }
    else {
        cerr << "Ошибка подключения: " << error.message() << endl;
    }
    
    start_accept();
}

void Connector::add_session(shared_ptr<Session> session) {
    lock_guard<mutex> lock(sessions_mutex_);
    sessions_.insert(session);
}

void Connector::remove_session(shared_ptr<Session> session) {
    lock_guard<mutex> lock(sessions_mutex_);
    sessions_.erase(session);
}

void Connector::broadcast_message(const string& from, const string& to, const string& content) {
    lock_guard<mutex> lock(sessions_mutex_);
    json message = {
        {"type", "message"},
        {"from", from},
        {"to", to},
        {"content", content},
        {"timestamp", time(nullptr)}
    };

    for (const auto& session : sessions_) {
        session->send_response(message);
    }
}

void Connector::broadcast_group_message(const string& from, const string& group_name, const string& content) {
    lock_guard<mutex> lock(sessions_mutex_);
    auto members = db_handler_.get_group_members(group_name);

    json message = {
        {"type", "group_message"},
        {"from", from},
        {"to", group_name},
        {"content", content},
        {"timestamp", time(nullptr)}
    };

    for (const auto& session : sessions_) {
        // Проверяем, является ли пользователь участником группы
        if (std::find(members.begin(), members.end(), session->get_username()) != members.end()) {
            session->send_response(message);
        }
    }
}