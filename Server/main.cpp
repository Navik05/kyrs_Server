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
        boost::asio::io_context io_context;

        // ������������� ���� ������
        DatabaseHandler db_handler("database/chat.db");

        // ������ ������� �� �����
        Connector connector(io_context, port, db_handler);

        cout << "������ �������, ���� " << port <<endl;
        io_context.run();
    }
    catch (exception& e) {
        cerr << "����������: " << e.what() << endl;
        return 1;
    }

    return 0;
}