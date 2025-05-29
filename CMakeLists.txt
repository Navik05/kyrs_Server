cmake_minimum_required(VERSION 3.10)
project(Server)

set(CMAKE_CXX_STANDARD 20)

find_package(Boost 1.66 REQUIRED COMPONENTS system)
find_package(MySQL REQUIRED)
include_directories(${MYSQL_INCLUDE_DIRS})
target_link_libraries(chat_server ${MYSQL_LIBRARIES})

include_directories(
    include
    ${Boost_INCLUDE_DIRS}
    ${SQLite3_INCLUDE_DIRS}
)

add_executable(server
    src/main.cpp
    src/Session.cpp
    src/DatabaseHandler.cpp
    src/Connector.cpp
)

target_link_libraries(server
    ${Boost_LIBRARIES}
    MySQL::MySQL
    nlohmann_json::nlohmann_json
)