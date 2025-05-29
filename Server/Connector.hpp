#pragma once
#include <boost/asio.hpp>
#include <memory>
#include "DatabaseHandler.hpp"
#include "Session.hpp"

class Connector {
public:
    Connector(io_context& io_context,
        unsigned int port,
        DatabaseHandler& db_handler);

private:
    void start_accept();
    void handle_accept(shared_ptr<ip::tcp::socket> socket,
        const boost::system::error_code& error);

    io_context& io_context_;
    ip::tcp::acceptor acceptor_;
    DatabaseHandler& db_handler_;
};