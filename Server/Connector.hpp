#pragma once
#include <boost/asio.hpp>
#include <memory>
#include <unordered_set>
#include "DatabaseHandler.hpp"
#include "Session.hpp"

class Connector {
private:
    io_context& io_context_;
    ip::tcp::acceptor acceptor_;
    DatabaseHandler& db_handler_;
    unordered_set<shared_ptr<Session>> sessions_;
    mutex sessions_mutex_;

    void start_accept();
    void handle_accept(shared_ptr<ip::tcp::socket> socket, const boost::system::error_code& error);

public:
    Connector(io_context& io_context, unsigned int port, DatabaseHandler& db_handler);
    void broadcast_message(const string& from, const string& to, const string& content, bool is_group);
    void add_session(shared_ptr<Session> session);
    void remove_session(shared_ptr<Session> session);
};