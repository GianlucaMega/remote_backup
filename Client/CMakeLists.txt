cmake_minimum_required(VERSION 3.16)
project(Client)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS -pthread)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
#link_libraries(ssl crypto)

add_executable(Client main.cpp FileWatcher.h ../Common/Message.cpp ../Common/Message.h ../Utilities/base64.cpp ../Utilities/Utilities.cpp FileWatcher.cpp)

find_package(Boost REQUIRED COMPONENTS regex)
include_directories(${Boost_INCLUDE_DIRS})
find_package(OpenSSL REQUIRED)
target_link_libraries(Client OpenSSL::SSL OpenSSL::Crypto)

if(MINGW)
    target_link_libraries(Client ws2_32)
endif()