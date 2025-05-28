#include "Session.hpp"

Session::Session(shared_ptr<ip::tcp::socket> socket)
	:socket(move(socket)){}

void Session::do_read(){
	// Указатель на текущий объект
	auto self(shared_from_this());

	// Асинхронное чтение в динамический буфер
	async_read_until(*socket, dynamic_buffer(buffer_), "\n",
		[this, self](boost::system::error_code ec, size_t length) {
			if (!ec) {
				// Извлечение длины сообщения
				string header(buffer_.data(), length);
				// Удаление заголовка из буфера
				buffer_.erase(0, length);
				size_t msg_length = stoul(header);
				read_body(msg_length);
			}
		});
}

// Чтение тела сообщения указанной длины
void Session::read_body(size_t length){
	auto self(shared_from_this());
	// Асинхронное чтение length байт
	async_read(*socket,buffer(buffer_.data(), length),
		[this, self, length](boost::system::error_code ec, size_t) {
			if (!ec) {
				try {
					// Парсинг JSON из буфера
					json msg = json::parse(buffer_.data(), buffer_.data() + length);
					process_message(msg);
				}
				catch (const exception& e) {
					cerr << "Ошибка анализа JSON : " << e.what() << std::endl;
				}

				// Очистка буфера и повторное чтение
				buffer_.clear();
				do_read();
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
			json response = {
				{"type", "auth_response"},
				{"status", "success"},
				{"message", "Authentication processed"}
			};
			send_response(response);
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
		cerr << "Ошибка при обработке: " << e.what() << endl;
	}
}

// Отправка ответа
void Session::send_response(const json& response) {
	auto self(shared_from_this());
	// Перевод JSON в строку
	string response_str = response.dump() + "\n";
	// Создание заголовка с длиной сообщения
	string header = to_string(response_str.size()) + "\n";

	// Создание буферов для заголовки и тела сообщения
	vector<const_buffer> buffers;
	buffers.push_back(buffer(header));
	buffers.push_back(buffer(response_str));

	// Асинхронная отправка данных с обработкой ошибок
	async_write(*socket, buffers,
		[this, self](boost::system::error_code ec, size_t) {
		if (ec) {
			cerr << "Ошибка ввода: " << ec.message() << endl;
		}
	});
}