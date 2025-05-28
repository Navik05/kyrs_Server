#include "Session.hpp"
#include <iostream>
#include <boost/asio.hpp>
#include <memory>

using namespace boost::asio;
using namespace std;

// Прием входящих подключений
class Connector {
private:
    io_context& context;    //взаимодействие с ос
    ip::tcp::acceptor& acceptor;    //сетевой протокол
    shared_ptr<ip::tcp::socket> socket;    //объект ввода/вывода
    const unsigned int port;

    // Потоковое подключение
    void async_connect() {
        socket = make_shared<ip::tcp::socket>(context);
        acceptor.async_accept(*socket, [this](boost::system::error_code ec) {
            if (!ec) {
                join();
            }
            });
    }

    // Подключение клиента
    void join() {
        Session* session = new Session(socket);
        async_connect();
    }

public:
    Connector(io_context& context, ip::tcp::acceptor& acceptor, unsigned int port)
        : context{ context }, acceptor{ acceptor }, port{ port }
    {
        cout << "Сервер запущен порт " << port << endl;
        async_connect();
    }
};

int main(int argc, char* argv[]) {
    setlocale(LC_ALL, "ru");
    unsigned int port = 52777;
    io_context context;
    ip::tcp::acceptor acceptor(context, ip::tcp::endpoint(ip::tcp::v4(), port));
    Connector connector(context, acceptor, port);
    context.run();
    //try {
    //    if (argc != 2) {
    //        std::cerr << "Usage: server <port>\n";
    //        return 1;
    //    }

    //    boost::asio::io_context io_context;
    //    Server server(io_context, std::atoi(argv[1]));
    //    io_context.run();
    //}
    //catch (std::exception& e) {
    //    std::cerr << "Exception: " << e.what() << "\n";
    //}

    return 0;
}