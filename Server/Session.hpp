#pragma once
#include <boost/asio.hpp>
#include <memory>
#include <string>
#include <iostream>

using namespace boost::asio;
using namespace std;

class Session : public enable_shared_from_this<Session> {
private:
	shared_ptr<ip::tcp::socket> socket;
	enum { max_length = 1024 };
	char data[max_length];
public:
	Session(shared_ptr<ip::tcp::socket> socket);
	void do_read();
	void do_write(size_t length);
};