#include "Session.hpp"

Session::Session(shared_ptr<ip::tcp::socket> socket, DatabaseHandler& db_handler)
	:socket(move(socket)), db_handler_(db_handler) {}

void Session::start() {
	do_read();
}

void Session::do_read(){
	// ��������� �� ������� ������
	auto self(shared_from_this());
	// ��� �������� ����� ���������
	buffer_.resize(4);		

	// ����������� ������ � ������������ �����
	async_read(*socket, buffer(buffer_),
		[this, self](boost::system::error_code ec, size_t length) {
			if (!ec) {
				uint32_t msg_length = *reinterpret_cast<uint32_t*>(buffer_.data());
				read_body(msg_length);
			}
		});
}

// ������ ���� ��������� ��������� �����
void Session::read_body(size_t length){
	auto self(shared_from_this());
	buffer_.resize(length);

	// ����������� ������ length ����
	async_read(*socket,buffer(buffer_),
		[this, self](boost::system::error_code ec, size_t) {
			if (!ec) {
				try {
					// ������� JSON �� ������
					json msg = json::parse(buffer_);
					process_message(msg);
				}
				catch (const exception& e) {
					cerr << "������ �������� JSON : " << e.what() << std::endl;
				}

				// ��������� ������
				start();
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
			handle_auth(msg);
		} else if (type == "message") {
			handle_message(msg);
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
		cerr << "������ ��� ��������� ���������: " << e.what() << endl;
	}
}

void Session::handle_auth(const json& msg) {
	string username = msg["username"];
	string password_hash = msg["password_hash"];

	bool auth_result = db_handler_.authenticate_user(username, password_hash);

	json response = {
		{"type", "auth_response"},
		{"status", auth_result ? "success" : "failure"}
	};
	send_response(response);
}

void Session::handle_message(const json& msg)
{
}

// �������� ������
void Session::send_response(const json& response) {
	auto self(shared_from_this());
	// ������� JSON � ������
	string response_str = response.dump();
	uint32_t length = response_str.size();

	// �������� ������� ��� ��������� � ���� ���������
	vector<const_buffer> buffers;
	buffers.push_back(buffer(&length, sizeof(length)));
	buffers.push_back(buffer(response_str));

	// ����������� �������� ������ � ���������� ������
	async_write(*socket, buffers,
		[this, self](boost::system::error_code ec, size_t) {
		if (ec) {
			cerr << "������ �����: " << ec.message() << endl;
		}
	});
}