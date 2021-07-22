//
// Created by Matteo Pappadà on 18/11/2020.
//

#include "Server.h"

/**
 * Bool for errors on message received and for probe status
 */
auto msgErr = false, probeOp = false;


/**
 * Empty constructor
 * @param socket (it has to be initialized)
 */
Server::Server(boost::asio::ip::tcp::socket socket): socket(std::move(socket)){
}

/**
 * Constructor with parameters
 * @param clientName string for the username of the client
 * @param hashedPwd string for the hash of the pwd of the client
 * @param socket
 */
Server::Server(const std::string& clientName, std::string hashedPwd, boost::asio::ip::tcp::socket socket)
        : clientName(clientName), hashedPwd(std::move(hashedPwd)), socket(std::move(socket)) {

    if(authClient(this->clientName, this->hashedPwd)){
        // Client directory created if not exists
        if(!std::filesystem::is_directory(std::filesystem::path("../Root/" + this->clientName)))
            std::filesystem::create_directory("../Root/" + this->clientName);
        for(auto &file : std::filesystem::recursive_directory_iterator(std::filesystem::path("../Root/" + this->clientName)))
            this->paths[file.path().string()] = false;
        std::cout << "Authentication success: Hello, " << clientName << "!" << std::endl;
        std::cout << "Sending Response..." << std::endl;
        sendAck(1);
        std::cout << "Response sent." << std::endl;
    }
    else{
        std::cout << "Authentication failed!" << std::endl;
        std::cout << "Sending Response..." << std::endl;
        sendAck(0);
        std::cout << "Response sent." << std::endl;
        this->socket.close();
    }
}

/**
 * Perform authentication of the client
 * @param clientName string for the username of the client
 * @param hashedPwd string for the hash of the pwd of the client
 * @return 1 if success, 0 if failed
 */
int Server::authClient(const std::string& clientName, const std::string& hashedPwd) {

    std::ifstream ifs("../auth.txt");
    if(ifs.fail()) {
        std::cout << strerror(errno) << std::endl;
        return 0;
    }
    std::string str;

    while(!ifs.eof()) {
        ifs >> str;
        if (str == clientName) {
            ifs >> str;
            if (str == hashedPwd) {
                return 1;
            }
        }
        else {
            ifs >> str;
        }
    }

    return 0;
}

/**
 * Read a message from the socket
 * @return the obj Message read from the socket
 */
Message Server::readMessage() {

    Message mex{};
    std::vector<char> buf(MAX_MSG_LEN);
    boost::system::error_code err;
    this->socket.wait(boost::asio::socket_base::wait_read);
    boost::asio::read(this->socket, boost::asio::buffer(buf, MAX_MSG_LEN), err);
    if (err) {
        std::cout << err.message() << std::endl;
        msgErr = true;
        return mex;
    }
    int n = std::stoi(buf.data());
    buf.resize(n);
    this->socket.wait(boost::asio::socket_base::wait_read);
    boost::asio::read(socket, boost::asio::buffer(buf, n), err);
    if (err) {
        std::cout << err.message() << std::endl;
        msgErr = true;
        return mex;
    }

    mex.parseJSON(buf);

    return mex;
}

/**
 * Execute the operation specified in the opCode of the message
 * @param mex Message with info about the operation to be executed
 * @return 1 if success, 0 if fail
 */
int Server::executeOperation(const Message& mex) {

    int res = 0;

    switch(mex.getOpcode()){
        case null:
            break;
        case create_file: res = createFile(mex);
            break;
        case create_dir: res = createDir(mex);
            break;
        case rename_file: res = renameFile(mex);
            break;
        case rename_dir: res = renameDir(mex);
            break;
        case remove_entry: res = removeEntry(mex);
            break;
        case start_probe: res = probe(mex);
            break;
        case ping: res = 1;
            break;
        case ok:
            break;
        case error: this->socket.close(); res = 1;
            break;
        case login: authClient(mex.getFilePath(), mex.getFileData().data());
            break;
        default:
            break;
    }

    // Map re-creation done not during probe
    if(!probeOp) {
        std::unordered_map<std::string, bool> tmp;
        for (auto &file : std::filesystem::recursive_directory_iterator(std::filesystem::path("../Root/" + this->clientName +"/"))) {
            std::string s(file.path().string());
            //Paths in POSIX standard notation
            std::replace(s.begin(), s.end(), '\\', '/');
            tmp[s] = false;
        }
        this->paths = std::move(tmp);

        for (auto &path : this->paths)
            std::cout << path.first << std::endl;
    }

    //Ack for probe operation is disabled
    if(mex.getOpcode() != start_probe)
        sendAck(res);

    msgErr = false;

    return res;
}

/**
 * Send the ack to the client
 * @param value int with value 1 if ack = OK, 0 if ack = ERROR
 */
void Server::sendAck(int value) {

    boost::system::error_code err;
    if (value) {
        Message mex{};
        mex.setOpcode(ok);
        mex.setFilePath(this->clientName);
        std::string s("OK!");
        std::vector<char> v(s.data(), s.data() + s.size());
        mex.setFileData(v);
        this->socket.wait(boost::asio::socket_base::wait_write);
        boost::asio::write(this->socket, boost::asio::buffer(mex.getJSON()), err);
        if (err) {
            std::cout << err.message() << std::endl;
        }
    } else {
        Message mex{};
        mex.setOpcode(error);
        mex.setFilePath(this->clientName);
        std::string s("ERROR!");
        std::vector<char> v(s.data(), s.data() + s.size());
        mex.setFileData(v);
        this->socket.wait(boost::asio::socket_base::wait_write);
        boost::asio::write(this->socket, boost::asio::buffer(mex.getJSON()), err);
        if (err) {
            std::cout << err.message() << std::endl;
        }
    }
}

/**
 * Create the file specified in the message
 * @param message Message with the info about the file to be created
 * @return 1 if success, 0 if fail
 */
int Server::createFile(Message message) {

    int res = 0;
    std::ofstream ofs;
    std::string path("../Root/" + this->clientName + "/" + message.getFilePath());
    ofs.open(path, std::fstream::out | std::ios::binary | std::ios_base::ate);
    if(ofs.fail()){
        msgErr = true;
        std::cout << strerror(errno) << std::endl;
    }

    // Eop signals the end of the file transfer
    while(message.getOpcode() != eop) {
        if(!msgErr) {
            std::string hash = computeHash(message.getFileData());
            if (hash == message.getDataHash()) {
                ofs.write(message.getFileData().data(), message.getFileData().size());
                message = readMessage();
            } else {
                ofs.close();
                msgErr=true;
                std::error_code err;
                //Deleting incomplete files (due to errors)
                std::filesystem::path p(path);
                std::filesystem::remove_all(p, err);
                if (err) {
                    std::cout << err.message() << std::endl;
                }
                return res;
            }
        }
        else{
            /*If there was an error the server continues to receive the remaining messages
                on the socket since the ack is sent only at the end of the transfer
            */
            message = readMessage();
        }
    }
    ofs.close();

    if(msgErr)
        return res;

    // Insertion of the new path in the paths map if in probe
    if(probeOp){
        this->paths.insert({path, false});
    }

    res = 1;

    return res;
}

/**
 * Create the directory specified in the message
 * @param message Message with the info about the directory to be created
 * @return 1 if success, 0 if fail
 */
int Server::createDir(const Message& message) {

    int res = 0;
    std::error_code err;
    std::filesystem::path p("../Root/" + this->clientName + "/" + message.getFilePath());
    std::filesystem::create_directories(p, err);
    if(err){
        std::cout << err.message() << std::endl;
        return res;
    }

    if(probeOp){
        this->paths.insert({("../Root/" + this->clientName + "/" + message.getFilePath()), false});
    }

    res = 1;

    return res;
}

/**
 * Modify the file specified in the message
 * @param message Message with the info about the file to be modified
 * @return 1 if success, 0 if fail
 */
int Server::renameFile(const Message& message) {

    int res = 0;
    std::error_code err;
    std::filesystem::path oldP("../Root/" + this->clientName + "/" + message.getFilePath());
    std::filesystem::path newP("../Root/" + this->clientName + "/" + message.getFileData().data());
    if(!std::filesystem::is_regular_file(oldP)){
        std::cout << "Not a regular file" << std::endl;
        return res;
    }
    std::filesystem::rename(oldP, newP, err);
    if(err){
        std::cout << err.message() << std::endl;
        return res;
    }
    res = 1;

    return res;
}

/**
 * Modify the directory specified in the message
 * @param message Message with the info about the directory to be modified
 * @return 1 if success, 0 if fail
 */
int Server::renameDir(const Message& message) {

    int res = 0;
    std::error_code err;
    std::filesystem::path oldP("../Root/" + this->clientName + "/" + message.getFilePath());
    std::filesystem::path newP("../Root/" + this->clientName + "/" + message.getFileData().data());
    if(!std::filesystem::is_directory(oldP)){
        std::cout << "Not a directory" << std::endl;
        return res;
    }
    std::filesystem::rename(oldP, newP, err);
    if(err){
        std::cout << err.message() << std::endl;
        return res;
    }
    res = 1;

    return res;
}

/**
 * Remove the file/directory specified in the message
 * @param message Message with the info about the file/directory to be removed
 * @return 1 if success, 0 if fail
 */
int Server::removeEntry(const Message& message) {

    int res = 0;
    std::error_code err;
    std::filesystem::path p("../Root/" + this->clientName + "/" + message.getFilePath());
    std::filesystem::remove_all(p, err);
    if(err){
        std::cout << err.message() << std::endl;
        return res;
    }

    if(probeOp){
        auto it = this->paths.find("../Root/" + this->clientName + "/" + message.getFilePath());
        if(it != this->paths.end())
            this->paths.erase(it->first);
    }

    res = 1;

    return res;
}

/**
 * Method for the probe command of client
 * @return
 */
int Server::probe(Message message) {
    probeOp = true;
    std::string path;
    std::unordered_map<std::string, bool>::iterator it;

    message = readMessage();
    while(message.getOpcode() != eop){
        path = "../Root/" + this->clientName + "/" + message.getFilePath();
        it = this->paths.find(path);
        if(message.getOpcode() == check_file) {
            if (it != this->paths.end() && computeFileHash(path) == message.getFileData().data()) {
                // File is present in the server
                it->second = true;
                sendAck(1);
            } else {
                // File not present in the server
                sendAck(0);
            }
        }
        else if(message.getOpcode() == check_dir){
            if (it != this->paths.end()) {
                it->second = true;
                sendAck(1);
            } else {
                sendAck(0);
            }
        }
        message = readMessage();
    }

    message = readMessage();
    int res;
    // Receiving operations for untracked entries
    while(message.getOpcode() != eop){
        res = executeOperation(message);
        path = "../Root/" + this->clientName + "/" + message.getFilePath();
        it = this->paths.find(path);
        if(it != this->paths.end()) {
            if (res) {
                //File marked as valid
                it->second = true;
            } else {
                // File marked as invalid
                it->second = false;
            }
        }
        message = readMessage();
    }

    std::error_code err;
    std::filesystem::path p;
    // Deletion of the entries that are not present in the client
    for(const auto& a: paths){
        if(!a.second){
            p = a.first;
            std::filesystem::remove_all(p, err);
            if(err){
                std::cout << err.message() << std::endl;
            }
            else{
                paths.erase(a.first);
            }
        }
    }
    probeOp = false;
    return 1;
}

/**
 * Check if socket of the server is open
 * @return True if is open, False if not
 */
bool Server::socketIsOpen(){
    return this->socket.is_open();
}

/**
 * Close the socket (if is opened)
 */
void Server::closeSocket() {
    if(socketIsOpen())
        this->socket.close();
}

/**
 * Getter for clientName
 * @return clientName
 */
const std::string &Server::getClientName() const {
    return clientName;
}

/**
 * Getter for hashedPwd
 * @return hashedPwd
 */
const std::string &Server::getHashedPwd() const {
    return hashedPwd;
}

/**
 * Getter for readable bytes on the socket
 * @return readable bytes
 */
boost::asio::ip::tcp::socket::bytes_readable Server::getIOControl() {
    boost::asio::ip::tcp::socket::bytes_readable b;
    this->socket.io_control(b);
    return b;
}

/**
 * Setter for clientName
 * @param clientName string with the value to be assigned to clientName
 */
void Server::setClientName(const std::string &clientName) {
    this->clientName = clientName;
}

/**
 * Setter for hashedPwd
 * @param hashedPwd string with the value to be assigned to hashedPwd
 */
void Server::setHashedPwd(const std::string &hashedPwd) {
    this->hashedPwd = hashedPwd;
}

/**
 * Setter for socket
 * @param socket boost socket with to be moved into server socket
 */
void Server::setSocket(boost::asio::ip::tcp::socket socket) {
    this->socket = std::move(socket);
}