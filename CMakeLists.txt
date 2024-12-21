cmake_minimum_required(VERSION 3.2)
project(UDPScalableServer)

set(CMAKE_CXX_STANDARD 17)

add_executable(UDPScalableServer main.cpp
        UDPServer.cpp
        UDPServer.h)
