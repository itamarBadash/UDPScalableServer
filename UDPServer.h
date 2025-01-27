#pragma once
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <functional>
#include <netinet/in.h>

// Export macro for symbol visibility
#ifdef EM_SERVERS_EXPORTS
#define EM_API __attribute__((visibility("default")))
#else
#define EM_API
#endif

class EM_API UDPServer {
public:
    UDPServer(int port, int numSockets);
    ~UDPServer();

    UDPServer(const UDPServer&) = delete;
    UDPServer& operator=(const UDPServer&) = delete;

    UDPServer(UDPServer&&) noexcept = default;
    UDPServer& operator=(UDPServer&&) noexcept = default;

    // Public API
    bool start();
    void stop();
    void registerCommandCallback(std::function<void(const std::vector<uint8_t>&, const sockaddr_in&)> callback);
    void sendToClient(const std::vector<uint8_t>& message, const sockaddr_in& clientAddr);
    void sendToAllClients(const std::vector<uint8_t>& message);

private:
    // Internal methods
    void workerThreadFunction(int socket);
    void processCommand();
    void enqueueTask(std::function<void()> task);
    void workerThread();
    bool isClientRegistered(const sockaddr_in& clientAddr);

    // Configuration
    int port;
    int numSockets;

    // Resources
    std::vector<int> serverSockets;
    std::vector<std::thread> threads;
    std::vector<std::thread> workerThreads;
    std::thread commandProcessorThread;

    // State flags
    bool running;
    bool bstop;

    // Client data
    std::vector<sockaddr_in> clients;

    // Command and task queues
    std::queue<std::pair<std::vector<uint8_t>, sockaddr_in>> commandQueue;
    std::queue<std::function<void()>> taskQueue;

    // Thread synchronization
    std::mutex queueMutex;
    std::condition_variable queueCondition;
    std::condition_variable taskCondition;

    // Callback
    std::function<void(const std::vector<uint8_t>&, const sockaddr_in&)> commandCallback;
};
