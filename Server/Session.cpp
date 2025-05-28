#include "Session.hpp"

Session::Session(shared_ptr<ip::tcp::socket> socket)
	:socket(move(socket)){}

void Session::do_read(){
	// ��������� �� ������� ������
	auto self(shared_from_this());

	// ����������� ������ � ������������ �����
	async_read_until(*socket, dynamic_buffer(buffer_), "\n",
		[this, self](boost::system::error_code ec, size_t length) {
			if (!ec) {
				// ���������� ����� ���������
				string header(buffer_.data(), length);
				// �������� ��������� �� ������
				buffer_.erase(0, length);
				size_t msg_length = stoul(header);
				read_body(msg_length);
			}
		});
}

// ������ ���� ��������� ��������� �����
void Session::read_body(size_t length){
	auto self(shared_from_this());
	// ����������� ������ length ����
	async_read(*socket,buffer(buffer_.data(), length),
		[this, self, length](boost::system::error_code ec, size_t) {
			if (!ec) {
				try {
					// ������� JSON �� ������
					json msg = json::parse(buffer_.data(), buffer_.data() + length);
					process_message(msg);
				}
				catch (const exception& e) {
					cerr << "������ ������� JSON : " << e.what() << std::endl;
				}

				// ������� ������ � ��������� ������
				buffer_.clear();
				do_read();
			}
		});
}

// ��������� ���������
void Session::process_message(const json& msg){
	try {
		// ���������� ���� ��������� �� JSON
		string type = msg["type"];

		// ��������� ��������������
		if (type == "auth") {
			json response = {
				{"type", "auth_response"},
				{"status", "success"},
				{"message", "Authentication processed"}
			};
			send_response(response);
		}
		// ��������� ����������� ����� ���������
		else {
			json response = {
				{"type", "error"},
				{"message", "Unknown message type"}
			};
			send_response(response);
		}
	}
	// ��������� ������ ��������
	catch (const exception& e) {
		cerr << "������ ��� ���������: " << e.what() << endl;
	}
}

// �������� ������
void Session::send_response(const json& response) {
	auto self(shared_from_this());
	// ������� JSON � ������
	string response_str = response.dump() + "\n";
	// �������� ��������� � ������ ���������
	string header = to_string(response_str.size()) + "\n";

	// �������� ������� ��� ��������� � ���� ���������
	vector<const_buffer> buffers;
	buffers.push_back(buffer(header));
	buffers.push_back(buffer(response_str));

	// ����������� �������� ������ � ���������� ������
	async_write(*socket, buffers,
		[this, self](boost::system::error_code ec, size_t) {
		if (ec) {
			cerr << "������ �����: " << ec.message() << endl;
		}
	});
}