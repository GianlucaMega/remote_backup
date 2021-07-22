#include "FileWatcher.h"


/**
 * Constructor with all parameters
 * @param sock socket
 * @param delay sleep time of the watcher
 */
FileWatcher::FileWatcher(boost::asio::ip::tcp::socket sock, std::chrono::duration<int, std::milli> delay) : socket{std::move(sock)}, delay{delay} {

    if(!clientLogin()) {
        running_ = false; //It's a fatal error -> start() is automatically blocked and the client ends
        return;
    }
    // Creation of the maps
    for(auto &file : std::filesystem::recursive_directory_iterator(this->path_to_watch)) {
        paths_[file.path().string()] = std::filesystem::last_write_time(file);
        trace_map[file.path().string()]= {'I', (std::filesystem::is_directory(file.path()) ? FileStatus::dir_created : FileStatus::modified)};
    }
    for(auto & m: trace_map)
        std::cout << m.first << " " << m.second.first << std::endl;

    probe();

}

/**
 * Routine of the file watcher
 */
void FileWatcher::start() {
    bool result;
    while(running_) {
        // Wait for "delay" milliseconds
        std::this_thread::sleep_for(delay);
        loops++;

        auto it = paths_.begin();
        while (it != paths_.end()) {
            if (!std::filesystem::exists(it->first)) {
                std::cout << "Erased " << it->first <<std::endl;
                trace_map[it->first]={'I', FileStatus::erased};
                result=sendMessage(FileStatus::erased,it->first);
                if(result){
                    std::cout<<"Server ok!"<<std::endl;
                    auto del=trace_map.find(it->first);
                    if(del!=trace_map.end())
                        trace_map.erase(del);
                    it = paths_.erase(it);
                }
                else
                    std::cout<<"Server error!"<<std::endl;
            } else {
                it++;
            }
        }

        // Check if a file was created or modified
        for (auto &file : std::filesystem::recursive_directory_iterator(path_to_watch)) {
            auto current_file_last_write_time = std::filesystem::last_write_time(file);

            // File creation
            if (!contains(file.path().string())) {
                paths_[file.path().string()] = current_file_last_write_time;
                if (!fs::is_directory(file.path().string())) {
                    std::cout << "File created: " << file.path().string() << " Size: " << fs::file_size(file.path().string())<<std::endl;
                    trace_map.insert({file.path().string(), std::make_pair('I', FileStatus::created)});

                    result=sendMessage(FileStatus::created,file.path().string());
                    if(result){
                        trace_map[file.path().string()].first='V';
                        std::cout<<"Server ok!"<<std::endl;
                    }
                    else
                        std::cout<<"Server error!"<<std::endl;

                }
                else {
                    std::cout << "Directory created: " << file.path().string() <<std::endl;
                    result=sendMessage(FileStatus::dir_created, file.path().string());
                    trace_map.insert({file.path().string(), std::make_pair('I',FileStatus::dir_created)});
                    if(result){
                        trace_map[file.path().string()].first='V';
                        std::cout<<"Server ok!"<<std::endl;
                    }
                    else
                        std::cout<<"Server error!"<<std::endl;
                }
            }

            // File modification
            else {
                if(paths_[file.path().string()] != current_file_last_write_time) {
                    paths_[file.path().string()] = current_file_last_write_time;
                    if (!fs::is_directory(file.path().string())) {
                        std::cout<<"File modified: "<<file.path().string()<<std::endl;
                        trace_map[file.path().string()]={'I', FileStatus::modified};
                        result=sendMessage(FileStatus::modified, file.path().string());
                        if(result){
                            trace_map[file.path().string()].first='V';
                            std::cout<<"Server ok!"<<std::endl;
                        }
                        else
                            std::cout<<"Server error!"<<std::endl;
                    }
                }
            }
        }

        if( (sockerr && checkConnection()) || serverr || loops>=PROBETIME){
            if(loops>=PROBETIME)
                loops=0;
            for(auto &m: trace_map)
                m.second.first='I';
            probe();
        }
    }
}

/**
 * Login of the client
 * @return true if success, false instead
 */
bool FileWatcher::clientLogin(){
    std::error_code ec;
    boost::system::error_code err;
    err.clear();
    std::string line,user,pass;
    Message mex{};
    bool rewrite=false;
    int cnt=0;

    std::ifstream infile(CONF_FILE_CLIENT);
    if(infile.fail()){
        std::cout<<"Error in configuration file opening: "<<strerror(errno)<<std::endl;
        socket.close();
        return false;
    }

    while(!infile.eof()){
        infile>>line;
        if(line=="USER")
            infile>>user;
        else if(line=="PASS")
            infile>>pass;
        else if(line=="PATH")
            infile>>path_to_watch;
    }
    infile.close();

    std::filesystem::recursive_directory_iterator iter{path_to_watch,ec};
    while(ec){
        std::cout << "Incorrect/unknown path inserted, try again (without the final '/'): ";
        std::getline(std::cin,this->path_to_watch);
        iter=std::filesystem::recursive_directory_iterator{this->path_to_watch,ec};
        rewrite=true;
    }

    bool printErr = false;
    //Login
    do{
        if(printErr){
            std::cout<<"Socket error, trying to resume connection."<<std::endl;
            printErr = false;
        }

        if (err || mex.getOpcode()==error){
            socket.close();
            socket.connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(IP_SERVER), PORT_NUM),err);
            if(err){
                printErr = true;
                continue;
            }
        }

        mex=Message{login,user,std::vector<char>(pass.begin(),pass.end())};
        boost::asio::write(socket,boost::asio::buffer(mex.getJSON()),err);
        if(err){
            printErr = true;
            continue;
        }

        std::vector<char> buf(MAX_MSG_LEN);
        boost::asio::read(socket,boost::asio::buffer(buf, MAX_MSG_LEN), err);
        if(err){
            printErr = true;
            continue;
        }

        int n = std::stoi(buf.data());
        buf.resize(n);
        boost::asio::read(socket, boost::asio::buffer(buf, n), err);
        if(err){
            printErr = true;
            continue;
        }

        mex.parseJSON(buf);
        if(mex.getOpcode()==error){
            rewrite=true;
            std::cout<<"Login error!"<<std::endl<<"Insert username: ";
            std::cin>>user;
            std::cout<<"Insert password: ";
            std::cin>>pass;
        } else
            std::cout<<"Login success!"<<std::endl;
        cnt++;
    } while((mex.getOpcode()==error || err) && cnt<5);

    if(rewrite && cnt<5){
        std::ofstream outfile("../client.conf");
        if(outfile.fail()){
            std::cout<<"Error in configuration file opening: "<<strerror(errno)<<std::endl;
            socket.close();
            return false;
        }
        outfile<<"USER"<<std::endl<<user<<std::endl;
        outfile<<"PASS"<<std::endl<<pass<<std::endl;
        outfile<<"PATH"<<std::endl<<path_to_watch;
        outfile.close();
    }

    return cnt < 5;
}

/**
 * Method for re-establish a connection with the server after a connection problem
 * @return true if success, false instead
 */
bool FileWatcher::checkConnection(){
    Message msg{};
    boost::system::error_code err;

    // Try with a ping to check if connection is already re-established
    msg.setOpcode(ping);
    boost::asio::write(socket, boost::asio::buffer(msg.getJSON()), err);

    boost::asio::ip::tcp::socket::bytes_readable b;
    socket.io_control(b);
    // If we get a response for the ping, we clean the socket and return true
    if(b.get() > 0) {
        std::vector<char> buf(MAX_MSG_LEN);
        boost::asio::read(socket, boost::asio::buffer(buf, MAX_MSG_LEN), err);
        int n = std::stoi(buf.data());
        buf.resize(n);
        boost::asio::read(socket, boost::asio::buffer(buf, n), err);
        msg.parseJSON(buf);
        return true;
        // Else we try to close and connect again the socket
    } else{
        socket.close();
        socket.connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(IP_SERVER), PORT_NUM),err);
        if(err){
            return false;
        } else {
            return clientLogin();
        }
    }
}

/**
 * Method for sending a message to the server, basing on the parameters
 * @param status is the type of operation to be done
 * @param path is the path of the entry on which execute the operation
 * @return true if server ack is OK, false if it is "Error"
 */
bool FileWatcher::sendMessage(FileStatus status, const std::string& path){
    Message mex{};
    boost::system::error_code err;
    // Operation for the probe method
    if(status == FileStatus::check){
        if(std::filesystem::is_directory(path)){
            mex=Message{check_dir,path.substr(path_to_watch.size()+1)};

        } else{
            std::string filehash=computeFileHash(path);
            mex=Message{check_file, path.substr(path_to_watch.size()+1),std::vector(filehash.begin(),filehash.end())};
        }
        boost::asio::write(socket, boost::asio::buffer(mex.getJSON()), err);
        if(err){
            std::cout<<"Socket error, connection will be resumed soon. All file modifications are monitored and saved."<<std::endl;
            sockerr=true;
            return false;
        }
    }
    // Operation for create a directory
    if(status == FileStatus::dir_created){
        mex=Message{create_dir, path.substr(path_to_watch.size()+1)};
        boost::asio::write(socket, boost::asio::buffer(mex.getJSON()), err);
        if(err){
            std::cout<<"Socket error, connection will be resumed soon. All file modifications are monitored and saved."<<std::endl;
            sockerr=true;
            return false;
        }
    }
    // Operation for erase an entry, also for modified (= erase + create)
    if(status == FileStatus::erased || status == FileStatus::modified){
        mex=Message{remove_entry, path.substr(path_to_watch.size()+1)};
        boost::asio::write(socket, boost::asio::buffer(mex.getJSON()), err);
        if(err){
            std::cout<<"Socket error, connection will be resumed soon. All file modifications are monitored and saved."<<std::endl;
            sockerr=true;
            return false;
        }
    }
    // Operation for create a file, also for modified (= erase + create)
    if((status == FileStatus::created || status == FileStatus::modified)) {
        std::ifstream in;
        in.open(path, std::fstream::in | std::ios::binary | std::ios_base::ate);
        if(in.fail()){
            std::cout<<"Error on opening file: "<<path<<std::endl;
            serverr=true;
            return false;
        }

        int size = std::filesystem::file_size(path);
        int read = 0;

        //Empty file created
        if(size==0){
            mex=Message{create_file, path.substr(path_to_watch.size() + 1), std::vector<char>{}};
            boost::asio::write(socket, boost::asio::buffer(mex.getJSON()),err);
            if(err){
                std::cout<<"Socket error, connection will be resumed soon. All file modifications are monitored and saved."<<std::endl;
                sockerr=true;
                return false;
            }
        } else {
            //Non empty file created
            while (read < size) {
                int rem = size - read;
                int buffersize = (rem < MAX_BODY_LEN) ? rem : MAX_BODY_LEN;
                char buf[MAX_BODY_LEN];
                in.seekg(read, std::ios_base::beg);
                in.read(buf, buffersize);
                read += buffersize;
                std::vector<char> vec(buf, buf + buffersize);
                mex = Message{create_file, path.substr(path_to_watch.size() + 1), vec};
                boost::asio::write(socket, boost::asio::buffer(mex.getJSON()), err);
                if(err){
                    std::cout<<"Socket error, connection will be resumed soon. All file modifications are monitored and saved."<<std::endl;
                    sockerr=true;
                    return false;
                }
            }
        }

        //Signal end of file transfer
        sendEOP(path);
    }

    int i=(status==FileStatus::modified ? 0 : 1);
    bool msgerr=false;
    //Ack receiving
    do{
        std::vector<char> buf(MAX_MSG_LEN);
        boost::asio::read(socket,boost::asio::buffer(buf, MAX_MSG_LEN), err);
        if(err){
            std::cout<<"Socket error, connection will be resumed soon. All file modifications are monitored and saved."<<std::endl;
            sockerr=true;
            return false;
        }
        int n = std::stoi(buf.data());
        buf.resize(n);
        boost::asio::read(socket, boost::asio::buffer(buf, n), err);
        if(err){
            std::cout<<"Socket error, connection will be resumed soon. All file modifications are monitored and saved."<<std::endl;
            sockerr=true;
            return false;
        }
        mex.parseJSON(buf);
        if(mex.getOpcode() == error){
            msgerr=true;
            serverr=true;
        }
        i++;
    }while(i<2); // If the status is modified (= erase + create), so 2 ack received

    return !msgerr;
}

/**
 * Method for sending the eop (end of operation)
 * @param path path for the operation done
 */
void FileWatcher::sendEOP(const std::string& path){
    boost::system::error_code err;
    Message mex{eop,path.substr(path_to_watch.size() + 1)};
    boost::asio::write(socket, boost::asio::buffer(mex.getJSON()), err);
    if(err){
        std::cout<<"Socket error, connection will be resumed soon. All file modifications are monitored and saved."<<std::endl;
        sockerr=true;
    }
}

/**
 * Method for the probe command (to sync server with client)
 */
void FileWatcher::probe(){
    bool result;
    Message mex{};
    boost::system::error_code err;
    sockerr=serverr=false;

    // Start probe signal
    mex.setOpcode(start_probe);
    boost::asio::write(socket, boost::asio::buffer(mex.getJSON()), err);
    if(err){
        std::cout<<"Socket error, connection will be resumed soon. All file modifications are monitored and saved."<<std::endl;
        sockerr=true;
        return;
    }

    // First phase: check message to server (with file hash if file). If the entry corresponds, server returns ok, error instead
    for(auto &m: trace_map) {
        if(m.second.first != 'V') {
            result = sendMessage(FileStatus::check, m.first);
            if (result)
                // Set to VALID if an entry is valid
                m.second.first='V';
        }
    }

    // Signaling end of check phase
    sendEOP(path_to_watch + "/eop");

    // Second phase: sending operations to sync the server
    for(auto &m: trace_map){
        //Process only INVALID entries
        if(m.second.first=='V')
            continue;

        result = sendMessage(m.second.second,m.first);
        if(result)
            // If server returns OK, set to VALID
            m.second.first='V';
    }

    //Signaling end of sync phase
    sendEOP(path_to_watch + "/eop");

    // Third phase: erase the erased entry from the maps
    for(auto &m: trace_map){
        if(m.second.second == FileStatus::erased && m.second.first=='V'){
            auto it=paths_.find(m.first);
            if(it!=paths_.end())
                paths_.erase(it);
            trace_map.erase(m.first);
        }
    }
}

/**
 * Method to check if a key exist inside the map
 * @param key is the key to search for
 * @return true if found, false if not
 */
bool FileWatcher::contains(const std::string &key) {
    auto el = paths_.find(key);
    return el != paths_.end();
}