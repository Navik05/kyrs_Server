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
        // Создание контекста ввода-вывода 
        io_context context;

        // Создание объекта БД
        DatabaseHandler db_handler(
            "127.0.0.1",
            "chat_user",
            "chat_password",
            "chat_db",
            3306
        );

        // Общий указатель на сервер
        auto connector = make_shared<Connector>(context, port, db_handler);
        cout << "Сервер запущен, порт " << port << endl;
        context.run();
    }
    catch (exception& e) {
        cerr << "Исключение: " << e.what() << endl;
        return 1;
    }
}