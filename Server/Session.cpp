#include "Session.hpp"

Session::Session(shared_ptr<ip::tcp::socket> socket)
	:socket(move(socket)){}

void Session::do_read()
{
	auto self(shared_from_this());
	socket->async_read_some(buffer(data, max_length),
		[this, self](boost::system::error_code ec, size_t length) {
			if (!ec) {
				cout << "Получено: " << string(data, length) << endl;
				do_write(length);
			}
		});
}

void Session::do_write(size_t length)
{
	auto self(shared_from_this());
	async_write(*socket, buffer(data, length),
		[this, self](boost::system::error_code ec, size_t length) {
			if (!ec) {
				do_read();
			}
		});
}