#pragma once
#include <boost/asio.hpp>
#include <memory>
#include <unordered_set>
#include "DatabaseHandler.hpp"
#include "Session.hpp"

class Connector {
public:
    Connector(io_context& io_context,
        unsigned int port,
        DatabaseHandler& db_handler);

    void add_session(shared_ptr<Session> session);
    void remove_session(shared_ptr<Session> session);
    void broadcast_message(const string& from, const string& to, const string& content);
    void broadcast_group_message(const string& from, const string& group_name, const string& content);

private:
    void start_accept();
    void handle_accept(shared_ptr<ip::tcp::socket> socket,
        const boost::system::error_code& error);

    io_context& io_context_;
    ip::tcp::acceptor acceptor_;
    DatabaseHandler& db_handler_;
    unordered_set<shared_ptr<Session>> sessions_;
    mutex sessions_mutex_;
};