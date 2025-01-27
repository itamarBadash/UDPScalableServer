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

// Export macro for symbol visibility
#ifdef EM_SERVERS_EXPORTS
#define EM_API __attribute__((visibility("default")))
#else
#define EM_API
#endif

class EM_API TCPServer {
public:
    TCPServer(int port, int numSockets = 1);
    ~TCPServer();

    TCPServer(const TCPServer&) = delete;
    TCPServer& operator=(const TCPServer&) = delete;

    TCPServer(TCPServer&&) noexcept = default;
    TCPServer& operator=(TCPServer&&) noexcept = default;

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
    std::vector<std::thread> clientThreads;   // Threads for each client connection

    std::queue<std::pair<std::vector<uint8_t>, sockaddr_in>> commandQueue; // Command queue
    std::queue<std::function<void()>> taskQueue;                           // Task queue
    std::mutex queueMutex;                                                 // Mutex for command queue
    std::condition_variable queueCondition;                                // Condition variable for command queue
    std::condition_variable taskCondition;                                 // Condition variable for task queue
    std::thread commandProcessorThread;                                    // Command processor thread

    std::function<void(const std::vector<uint8_t>&, const sockaddr_in&)> commandCallback; // Command callback
    std::atomic<bool> bstop;                                                              // Stop flag for workers

    void workerThreadFunction(int serverSocket);
    void handleClient(int clientSocket); // Continuous message handler for a client
    void processCommand();
    void enqueueTask(std::function<void()> task);
    void workerThread();
    void cleanupThreads();
};
