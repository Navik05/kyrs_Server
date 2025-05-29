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

        // ������������� MySQL ���� ������
        DatabaseHandler db_handler("localhost",  // ����
            "chat_user",   // ������������
            "chat_password", // ������
            "chat_db",     // ��� ���� ������
            3306);         // ����

        // ������ ������� �� �����
        Connector connector(context, port, db_handler);

        cout << "������ �������, ���� " << port << endl;
        context.run();
    }
    catch (exception& e) {
        cerr << "����������: " << e.what() << endl;
        return 1;
    }

    return 0;
}