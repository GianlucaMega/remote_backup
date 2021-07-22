//
// Created by Matteo Pappadà on 26/01/21.
//

#pragma once

#include "Server.h"
#include <mutex>
#include <thread>
#include <chrono>

class ThreadPool {

    /**
     * Max number of thread in the pool
     */
    int maxThread;

    /**
     * Counter for the thread detached
     */
    static int counter;

    /**
     * Mutex for handling writing and reading on counter between threads
     */
    static std::mutex m;

public:

    explicit ThreadPool(int maxThread);

    bool newThread(Server server) const;

    static void endThread();

};

