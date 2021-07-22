cmake_minimum_required(VERSION 3.16)
project(Server)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

#link_libraries(ssl crypto)

add_executable(Server main.cpp Server.cpp ../Common/Message.cpp ../Utilities/base64.cpp ../Utilities/Utilities.cpp ThreadPool.cpp ThreadPool.h)

find_package(Boost REQUIRED COMPONENTS regex)
include_directories(${Boost_INCLUDE_DIRS})
find_package(OpenSSL REQUIRED)
target_link_libraries(Server OpenSSL::SSL OpenSSL::Crypto)

if(MINGW)
    target_link_libraries(Server ws2_32)
endif()

