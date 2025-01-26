#pragma once

#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <functional>
#include <netinet/in.h>
#include <atomic>
#include <map>

class TCPServer {
public:
    TCPServer(int port, int numSockets = 1);
    ~TCPServer();

    bool start();
    void stop();
    void registerCommandCallback(std::function<void(const std::vector<uint8_t>&, const sockaddr_in&)> callback);

    void sendToClient(const std::vector<uint8_t>& message, int clientSocket);
    void sendToAllClients(const std::vector<uint8_t>& message);

private:
    int port;
    int numSockets;
    std::atomic<bool> running;

    std::vector<int> serverSockets;           // Server sockets
    std::map<int, sockaddr_in> clientAddresses; // Map of client sockets to their sockaddr_in
    std::vector<int> clientSockets;           // List of connected client sockets
    std::vector<std::thread> listenerThreads; // Threads for each server socket
    std::vector<std::thread> workerThreads;   // Worker threads for task processing

    std::queue<std::pair<std::vector<uint8_t>, sockaddr_in>> commandQueue; // Command queue
    std::queue<std::function<void()>> taskQueue;                           // Task queue
    std::mutex queueMutex;                                                 // Mutex for command queue
    std::condition_variable queueCondition;                                // Condition variable for command queue
    std::condition_variable taskCondition;                                 // Condition variable for task queue
    std::thread commandProcessorThread;                                    // Command processor thread

    std::function<void(const std::vector<uint8_t>&, const sockaddr_in&)> commandCallback; // Command callback
    std::atomic<bool> bstop;                                                              // Stop flag for workers

    void workerThreadFunction(int serverSocket);
    void processCommand();
    void enqueueTask(std::function<void()> task);
    void workerThread();
    void cleanupThreads();
};
