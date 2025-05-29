#pragma once
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <iostream>
#include "DatabaseHandler.hpp"

using json = nlohmann::json;
using namespace boost::asio;
using namespace std;

class Connector; // Forward declaration

class Session : public enable_shared_from_this<Session> {
private:
    shared_ptr<ip::tcp::socket> socket;
    string buffer_;
    DatabaseHandler& db_handler_;
    weak_ptr<Connector> connector_;
    string username_;

public:
    Session(shared_ptr<ip::tcp::socket> socket, DatabaseHandler& db_handler);
    void start();
    void set_connector(shared_ptr<Connector> connector) { connector_ = connector; }
    void send_response(const json& response);
    string get_username() const { return username_; }

private:
    void do_read();
    void read_body(size_t length);
    void process_message(const json& msg);
    void handle_auth(const json& msg);
    void handle_message(const json& msg);
};