#include <iostream>
#include <boost/asio.hpp>
#include <memory>
#include "Connector.hpp"
#include "DatabaseHandler.hpp"

using namespace boost::asio;
using namespace std;

int main() {
    setlocale(LC_ALL, "ru");
    unsigned int port = 52777;

    try {
        io_context context;

        // Инициализация MySQL базы данных
        DatabaseHandler db_handler("localhost",  // хост
            "chat_user",   // пользователь
            "chat_password", // пароль
            "chat_db",     // имя базы данных
            3306);         // порт

        // Запуск сервера на порту
        Connector connector(context, port, db_handler);

        cout << "Сервер запущен, порт " << port << endl;
        context.run();
    }
    catch (exception& e) {
        cerr << "Исключение: " << e.what() << endl;
        return 1;
    }

    return 0;
}