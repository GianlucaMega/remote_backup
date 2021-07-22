#include <iostream>
#include <filesystem>
#include "../Common/Message.h"
#include "FileWatcher.h"

namespace fs = std::filesystem;
using boost::asio::ip::tcp;

int main() {

    try {
        boost::asio::io_context io_context;
        tcp::socket socket(io_context);
        socket.connect(tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), 3000));

        // Create a FileWatcher instance that will check the current folder for changes every 5 seconds
        FileWatcher fw{std::move(socket), std::chrono::milliseconds(DELAY)};

        // Start monitoring a folder for changes and (in case of changes)
        // propagate them to the server
        fw.start();

    }

    catch(std::exception& e)
    {
        std::cerr << e.what() <<std::endl;
    }
}