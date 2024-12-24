#pragma once
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <functional>
#include <netinet/in.h>

class UDPServer {
public:
    UDPServer(int port, int numSockets);
    ~UDPServer();

    bool start();
    void stop();
    void registerCommandCallback(std::function<void(const std::string&, const sockaddr_in&)> callback);

private:
    void workerThreadFunction(int socket);
    void processCommand();
    void enqueueTask(std::function<void()> task);
    void workerThread();

    int port;
    int numSockets;
    std::vector<int> serverSockets;
    std::vector<std::thread> threads;
    std::vector<std::thread> workerThreads;

    bool running;
    std::mutex queueMutex;
    std::queue<std::pair<std::string, sockaddr_in>> commandQueue;
    std::queue<std::function<void()>> taskQueue;
    std::condition_variable queueCondition;
    std::condition_variable taskCondition;
    std::thread commandProcessorThread;

    bool bstop;
    std::function<void(const std::string&, const sockaddr_in&)> commandCallback;
};