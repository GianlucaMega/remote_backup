//
// Created by Matteo Pappadà on 18/11/2020.
//

#pragma once

#include "../Common/Message.h"
#include "../Common/Parameters.h"
#include <vector>
#include <iostream>
#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <filesystem>
#include <utility>
#include <sys/types.h>
#include <sys/stat.h>
#include <boost/asio/ip/tcp.hpp>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>


class Server {

    /**
     * String for the username of the client
     */
    std::string clientName;

    /**
     * String for the username of the client
     */
    std::string hashedPwd;

    /**
     * Socket for the communication with the client
     */
    boost::asio::ip::tcp::socket socket;

    /**
     * Map for the image of the filesystem
     */
    std::unordered_map<std::string, bool> paths;


public:

    Server(boost::asio::ip::tcp::socket socket);

    Server(const std::string& clientName, std::string  hashedPwd, boost::asio::ip::tcp::socket socket);

    static int authClient(const std::string& clientName, const std::string& hashedPwd);

    Message readMessage();

    int executeOperation(const Message& mex);

    void sendAck(int value);

    int createFile(Message message);

    int createDir(const Message& message);

    int renameFile(const Message& message);

    int renameDir(const Message& message);

    int removeEntry(const Message& message);

    int probe(Message message);

    bool socketIsOpen();

    void closeSocket();

    const std::string &getClientName() const;

    const std::string &getHashedPwd() const;

    boost::asio::ip::tcp::socket::bytes_readable getIOControl();

    void setClientName(const std::string &clientName);

    void setHashedPwd(const std::string &hashedPwd);

    void setSocket(boost::asio::ip::tcp::socket socket);

};


