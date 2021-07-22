//
// Created by Matteo Pappadà on 26/01/21.
//

#include "ThreadPool.h"

int ThreadPool::counter;
std::mutex ThreadPool::m;

/**
 * Routine of the instantiated server
 * @param s Server object
 */
/*
 * Start and stop are used to close the server if it doesn't receive
 * any message for a time interval equal to DELAY*PROBETIME*INT_NUM
 */
void routineServer(Server s){
    auto start=std::chrono::steady_clock::now();
    while(s.socketIsOpen()) {
        auto stop=std::chrono::steady_clock::now();
        if(((stop-start)/std::chrono::milliseconds(1))>(DELAY*PROBETIME*INT_NUM)){
            s.closeSocket();
            break;
        }
        boost::asio::ip::tcp::socket::bytes_readable b;
        b = s.getIOControl();
        if(b.get() > 0) {
            start=std::chrono::steady_clock::now();
            Message mex = s.readMessage();
            int res = s.executeOperation(mex);
            if (res)
                std::cout << "Operation " << getActionString(mex.getOpcode()) << " completed successfully!" << std::endl;
            else
                std::cout << "Error in operation " << getActionString(mex.getOpcode()) << "!" << std::endl;
        }
    }
    std::cout << "Socket closed!" << std::endl;
    ThreadPool::endThread();
}

/**
 * Constructor of the thread pool
 * @param maxThread max number of detachable threads
 */
ThreadPool::ThreadPool(int maxThread): maxThread(maxThread) {
    counter = 0;
}

/**
 * Create and detach a thread if the pool is not full
 * @param function the function to be passed to the thread
 * @return true if created, false if not
 */
bool ThreadPool::newThread(Server server) const {
    m.lock();
    if(counter >= this->maxThread) {
        m.unlock();
        std::cout << "COUNTER= " << counter << std::endl;
        server.closeSocket();
        return false;
    }

    try{
        std::thread (routineServer, std::move(server)).detach();
    } catch (std::system_error& err){
        m.unlock();
        return false;
    }

    counter += 1;
    m.unlock();
    std::cout << "COUNTER= " << counter << std::endl;
    return true;
}

/**
 * Cancel the finished thread from the pool
 */
void ThreadPool::endThread() {
    m.lock();
    counter -= 1;
    std::cout << "COUNTER= " << counter << std::endl;
    m.unlock();
}