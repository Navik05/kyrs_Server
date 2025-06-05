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

class Connector;

class Session : public enable_shared_from_this<Session> {
private:
    shared_ptr<ip::tcp::socket> socket;
    string buffer_;
    DatabaseHandler& db_handler_;
    weak_ptr<Connector> connector_;
    string username_;
    void do_read();
    void process_message(const json& msg);
    json handle_auth(const json& msg);
    void handle_message(const json& msg, bool is_team);

public:
    Session(shared_ptr<ip::tcp::socket> socket, DatabaseHandler& db_handler);
    void start();
    void set_connector(shared_ptr<Connector> connector) { connector_ = connector; }
    void send_response(const json& response);
    string get_username() const { return username_; }
    json get_available_chats();
};