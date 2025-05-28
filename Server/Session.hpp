#pragma once
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <iostream>

using json = nlohmann::json;
using namespace boost::asio;
using namespace std;

// ��������� ����������
class Session : public enable_shared_from_this<Session> {
private:
	shared_ptr<ip::tcp::socket> socket;		//��������� �� TCP-�����
	string buffer_;							//����� ��� �������� ������
public:
	Session(shared_ptr<ip::tcp::socket> socket);
	void do_read();
	void read_body(size_t length);
	void process_message(const json& msg);
	void send_response(const json& response);
};