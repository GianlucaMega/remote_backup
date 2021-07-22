#pragma once

#include <filesystem>
#include <chrono>
#include <thread>
#include <fcntl.h>
#include <iostream>
#include <filesystem>
#include <unistd.h>
#include <unordered_map>
#include <string>
#include <boost/asio.hpp>
#include "../Common/Message.h"
#include "../Common/Parameters.h"

namespace fs = std::filesystem;

// Define available file changes
enum class FileStatus {created, modified, erased, dir_created, check};

class FileWatcher {

public:
    std::string path_to_watch;

    // Time interval at which we check the base folder for changes
    std::chrono::duration<int, std::milli> delay;

    // Keep a record of files from the base directory and their last modification time
    FileWatcher(boost::asio::ip::tcp::socket sock, std::chrono::duration<int, std::milli> delay);

    // Monitor "path_to_watch" for changes and in case of a change propagate it to the server
    void start();

private:

    boost::asio::ip::tcp::socket socket;

    std::unordered_map<std::string, std::filesystem::file_time_type> paths_;

    std::unordered_map<std::string, std::pair<char, FileStatus>> trace_map;

    bool running_ = true, sockerr=false, serverr=false;

    int loops=0;

    bool clientLogin();

    bool checkConnection();

    bool sendMessage(FileStatus status, const std::string& path);

    void sendEOP(const std::string& path);

    void probe();

    // Check if "paths_" contains a given key
    // If your compiler supports C++20 use paths_.contains(key) instead of this function
    bool contains(const std::string &key);
};