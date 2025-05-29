#pragma once
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <iostream>
#include "DatabaseHandler.hpp"

using json = nlohmann::json;
using namespace boost::asio;
using namespace std;

// Обработка соединения
class Session : public enable_shared_from_this<Session> {
private:
	shared_ptr<ip::tcp::socket> socket;		//указатель на TCP-сокет
	string buffer_;							//буфер для хранения данных
	DatabaseHandler& db_handler_;

public:
	Session(shared_ptr<ip::tcp::socket> socket, DatabaseHandler& db_handle);
	void start();

private:
	void do_read();
	void read_body(size_t length);
	void process_message(const json& msg);
	void send_response(const json& response);

	// Реализуем все обработчики сообщений
	void handle_auth(const json& msg);
	void handle_message(const json& msg);
};