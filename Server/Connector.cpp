#include "Connector.hpp"

Connector::Connector(io_context& io_context,
    unsigned int port,
    DatabaseHandler& db_handler)
    : io_context_(io_context),                                      // Инициализация контекста ввода-вывода
    acceptor_(io_context, ip::tcp::endpoint(ip::tcp::v4(), port)),  // Инициализация акцептора
    db_handler_(db_handler) {                                       // Инициализация обработчика БД
    start_accept();                                                 // Начало ожидания подключений
}

void Connector::start_accept() {
    // Создание нового сокета для будущего подключения
    auto socket = make_shared<ip::tcp::socket>(io_context_);

    // Асинхронное ожидание нового подключения
    acceptor_.async_accept(*socket,
        [this, socket](const boost::system::error_code& error) {
            handle_accept(socket, error);
        });
}

void Connector::handle_accept(shared_ptr<ip::tcp::socket> socket, const boost::system::error_code& error) {
    if (!error) {
        cout << "Новое подключение от: " << socket->remote_endpoint().address().to_string() << endl;

        // Создание сессии для нового клиента
        auto session = make_shared<Session>(socket, db_handler_);
        session->set_connector(shared_from_this());
        add_session(session);
        session->start();
    }
    else {
        cerr << "Ошибка подключения: " << error.message() << endl;
    }

    // Ожидание новых подключений
    start_accept();
}

// Добавление сессии в множество
void Connector::add_session(shared_ptr<Session> session) {
    lock_guard<mutex> lock(sessions_mutex_);
    sessions_.insert(session);                  
}

// Удаление сессии из множества
void Connector::remove_session(shared_ptr<Session> session) {
    lock_guard<mutex> lock(sessions_mutex_);
    sessions_.erase(session);                   
}

void Connector::broadcast_message(const string& from, const string& target, const string& content, bool is_team){
    // Создание JSON-сообщения
    lock_guard<mutex> lock(sessions_mutex_);
    json message = {
        {"type", is_team ? "team_message" : "message"},
        {"from", from},                 
        {"to", target},
        {"content", content},          
        {"timestamp", time(nullptr)}    
    };

    // Определение получателей
    unordered_set<string> recipients;
    if (is_team){
        auto members = db_handler_.get_team_members(target);
        recipients.insert(members.begin(), members.end());
    }
    else {
        recipients.insert(from);
        recipients.insert(target);
    }

    // Рассылаем всем подходящим сессиям
    for (const auto& session : sessions_) {
        if (!session) continue;

        string username = session->get_username();
        if (recipients.count(username) > 0) {
            try {
                session->send_response(message);
            }
            catch (const exception& e) {
                cerr << "Ошибка отправки для " << username << ": " << e.what() << endl;
            }
        }
    }
}