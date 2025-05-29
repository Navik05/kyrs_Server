cmake_minimum_required(VERSION 3.10)
project(Server)

set(CMAKE_CXX_STANDARD 20)

find_package(Boost 1.66 REQUIRED COMPONENTS system)
find_package(SQLite3 REQUIRED)

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
    ${SQLite3_LIBRARIES}
    nlohmann_json::nlohmann_json
)