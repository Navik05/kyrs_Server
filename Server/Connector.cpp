#include "Connector.hpp"

Connector::Connector(io_context& io_context,
    unsigned int port,
    DatabaseHandler& db_handler)
    : io_context_(io_context),
    acceptor_(io_context, ip::tcp::endpoint(ip::tcp::v4(), port)),
    db_handler_(db_handler) {
    start_accept();
}

void Connector::start_accept() {
    auto socket = make_shared<ip::tcp::socket>(io_context_);

    acceptor_.async_accept(*socket,
        [this, socket](const boost::system::error_code& error) {
            handle_accept(socket, error);
        });
}

void Connector::handle_accept(shared_ptr<ip::tcp::socket> socket,
    const boost::system::error_code& error) {
    if (!error) {
        make_shared<Session>(socket, db_handler_)->start();
    }
    start_accept();
}