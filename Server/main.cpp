#include <iostream>
#include "Server.h"
#include "ThreadPool.h"

// Maximum number of running threads
#define MAX_NUM_THREAD 5


using boost::asio::ip::tcp;

int main() {

    ThreadPool tp{MAX_NUM_THREAD};

    boost::asio::io_context ioCtx;
    boost::system::error_code err;
    tcp::acceptor acceptor(ioCtx, tcp::endpoint(boost::asio::ip::address::from_string(IP_SERVER), PORT_NUM));

    while(true) {

        tcp::socket socket(ioCtx);
        std::cout << "Server waiting..." << std::endl;
        acceptor.accept(socket);

        std::vector<char> buf(MAX_MSG_LEN);
        Message mex;
        boost::asio::read(socket, boost::asio::buffer(buf, MAX_MSG_LEN), err);
        int n = std::stoi(buf.data());
        buf.resize(n);
        boost::asio::read(socket, boost::asio::buffer(buf, n), err);
        mex.parseJSON(buf);

        Server server{mex.getFilePath(), mex.getDataHash(), std::move(socket)};

        if (server.socketIsOpen()) {
            std::cout << "Socket is still opened." << std::endl;
        } else {
            std::cout << "Socket has been closed." << std::endl;
            continue;
        }

        if(tp.newThread(std::move(server)))
            std::cout << "Thread detached" << std::endl;
        else
            std::cout << "Pool full, cannot detach a thread" << std::endl;
    }
}

