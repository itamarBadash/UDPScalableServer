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

    std::vector<int> serverSockets;
    std::map<int, sockaddr_in> clientAddresses;
    std::vector<int> clientSockets;
    std::vector<std::thread> listenerThreads;
    std::vector<std::thread> clientThreads;

    std::queue<std::pair<std::vector<uint8_t>, sockaddr_in>> commandQueue;
    std::queue<std::function<void()>> taskQueue;
    std::mutex queueMutex;
    std::condition_variable queueCondition;
    std::condition_variable taskCondition;
    std::thread commandProcessorThread;
    std::function<void(const std::vector<uint8_t>&, const sockaddr_in&)> commandCallback;
    std::atomic<bool> bstop;

    void workerThreadFunction(int serverSocket);
    void handleClient(int clientSocket); // Continuous message handler for a client
    void processCommand();
    void enqueueTask(std::function<void()> task);
    void workerThread();
    void cleanupThreads();
};
