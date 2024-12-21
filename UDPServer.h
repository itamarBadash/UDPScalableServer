#pragma once
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <netinet/in.h>

class UDPServer {
public:
    UDPServer(int port, int numSockets);
    ~UDPServer();

    bool start();
    void stop();

private:
    void workerThreadFunction(int socket);
    void processCommand(const std::string& message, sockaddr_in clientAddr);

    int port;
    int numSockets;
    std::vector<int> serverSockets;
    std::vector<std::thread> threads;

    bool running;
    std::mutex queueMutex;
    std::queue<std::pair<std::string, sockaddr_in>> commandQueue;
    std::condition_variable queueCondition;
};
