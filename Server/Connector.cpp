#include "Connector.hpp"

Connector::Connector(io_context& io_context,
    unsigned int port,
    DatabaseHandler& db_handler)
    : io_context_(io_context),  // Инициализация контекста ввода-вывода
    acceptor_(io_context, ip::tcp::endpoint(ip::tcp::v4(), port)),  // Инициализация акцептора
    db_handler_(db_handler) {   // Инициализация обработчика БД
    start_accept();             // Начало ожидания подключений
}

void Connector::start_accept() {
    // Создание нового сокета для будущего подключения
    auto socket = make_shared<ip::tcp::socket>(io_context_);

    // Асинхронное ожидание нового подключения
    acceptor_.async_accept(*socket,
        [this, socket](const boost::system::error_code& error) {
            handle_accept(socket, error);   // Обработчик подключения
        });
}

void Connector::handle_accept(shared_ptr<ip::tcp::socket> socket,
    const boost::system::error_code& error) {
    if (!error) {
        // Логирование нового подключения
        cout << "Новое подключение от: "
            << socket->remote_endpoint().address().to_string()
            << endl;

        // Создание сессии для нового клиента
        auto session = make_shared<Session>(socket, db_handler_);
        add_session(session);
        session->start();
    }
    else {
        cerr << "Ошибка подключения: " << error.message() << endl;
    }
    
    // Ожидание новых подключений
    start_accept();
}

void Connector::add_session(shared_ptr<Session> session) {
    lock_guard<mutex> lock(sessions_mutex_);    // Блокировка для потокобезопасности
    sessions_.insert(session);                  // Добавление сессии в множество
}

void Connector::remove_session(shared_ptr<Session> session) {
    lock_guard<mutex> lock(sessions_mutex_);
    sessions_.erase(session);                   // Удаление сессии из множества
}

void Connector::broadcast_message(const string& from, const string& to, const string& content) {
    lock_guard<mutex> lock(sessions_mutex_);
    // Создание JSON-сообщения
    json message = {
        {"type", "message"},            // Тип сообщения
        {"from", from},                 // Отправитель
        {"to", to},                     // Получатель
        {"content", content},           // Содержимое
        {"timestamp", time(nullptr)}    // Временная метка
    };

    // Рассылка сообщения всем активным сессиям
    for (const auto& session : sessions_) {
        session->send_response(message);
    }
}

void Connector::broadcast_group_message(const string& from, const string& group_name, const string& content) {
    lock_guard<mutex> lock(sessions_mutex_);

    // Получение списка участников группы из БД
    auto members = db_handler_.get_group_members(group_name);

    // Создание JSON-сообщения для группы
    json message = {
        {"type", "group_message"},
        {"from", from},
        {"to", group_name},
        {"content", content},
        {"timestamp", time(nullptr)}
    };

    // Рассылка только участникам группы
    for (const auto& session : sessions_) {
        // Проверяем, является ли пользователь участником группы
        if (find(members.begin(), members.end(), session->get_username()) != members.end()) {
            session->send_response(message);
        }
    }
}