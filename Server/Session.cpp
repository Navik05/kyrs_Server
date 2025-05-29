#include "Session.hpp"

Session::Session(shared_ptr<ip::tcp::socket> socket, DatabaseHandler& db_handler)
	:socket(move(socket)), db_handler_(db_handler) {}

void Session::start() {
	do_read();
}

void Session::do_read(){
	// Указатель на текущий объект
	auto self(shared_from_this());
	// Для хранения длины сообщения
	buffer_.resize(4);		

	// Асинхронное чтение в динамический буфер
	async_read(*socket, buffer(buffer_),
		[this, self](boost::system::error_code ec, size_t length) {
			if (!ec) {
				uint32_t msg_length = *reinterpret_cast<uint32_t*>(buffer_.data());
				read_body(msg_length);
			}
		});
}

// Чтение тела сообщения указанной длины
void Session::read_body(size_t length){
	auto self(shared_from_this());
	buffer_.resize(length);

	// Асинхронное чтение length байт
	async_read(*socket,buffer(buffer_),
		[this, self](boost::system::error_code ec, size_t) {
			if (!ec) {
				try {
					// Парсинг JSON из буфера
					json msg = json::parse(buffer_);
					process_message(msg);
				}
				catch (const exception& e) {
					cerr << "Ошибка парсинга JSON : " << e.what() << std::endl;
				}

				// Повторное чтение
				start();
			}
		});
}

// Обработка сообщения
void Session::process_message(const json& msg){
	try {
		// Извлечение типа сообщения из JSON
		string type = msg["type"];

		// Обработка аутентификации
		if (type == "auth") {
			handle_auth(msg);
		} else if (type == "message") {
			handle_message(msg);
		}
		// Обработка неизвестных типов сообщений
		else {
			json response = {
				{"type", "error"},
				{"message", "Unknown message type"}
			};
			send_response(response);
		}
	}
	// Обработка ошибок парсинга
	catch (const exception& e) {
		cerr << "Ошибка при обработке сообщения: " << e.what() << endl;
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

// Отправка ответа
void Session::send_response(const json& response) {
	auto self(shared_from_this());
	// Перевод JSON в строку
	string response_str = response.dump();
	uint32_t length = response_str.size();

	// Создание буферов для заголовки и тела сообщения
	vector<const_buffer> buffers;
	buffers.push_back(buffer(&length, sizeof(length)));
	buffers.push_back(buffer(response_str));

	// Асинхронная отправка данных с обработкой ошибок
	async_write(*socket, buffers,
		[this, self](boost::system::error_code ec, size_t) {
		if (ec) {
			cerr << "Ошибка ввода: " << ec.message() << endl;
		}
	});
}