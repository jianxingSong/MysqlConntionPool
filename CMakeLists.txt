cmake_minimum_required(VERSION 3.21)
project(mysqlConnectPool)

set(CMAKE_CXX_STANDARD 14)

include_directories(/usr/include/mysql)

add_executable(mysqlConnectPool main.cpp mysqlConnectPool.h mysqlConnectPool.cpp)
target_link_libraries(mysqlConnectPool mysqlclient -ljsoncpp -lpthread)