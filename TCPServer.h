#ifndef UDPSCALABLESERVER_TCPSERVER_H
#define UDPSCALABLESERVER_TCPSERVER_H

#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <functional>
#include <netinet/in.h>
#include <atomic>

class TCPServer {
public:
    TCPServer(int port, int numSockets = 1); // Constructor with port and number of sockets
    ~TCPServer();

    bool start(); // Start the server
    void stop();  // Stop the server

    void registerCommandCallback(std::function<void(const std::vector<uint8_t>&, int)> callback); // Register callback
    void sendToClient(const std::vector<uint8_t>& message, int clientSocket); // Send to a specific client
    void sendToAllClients(const std::vector<uint8_t>& message); // Send to all connected clients

private:
    int port;                                 // Server port
    int numSockets;                           // Number of server sockets for concurrency
    std::atomic<bool> running;                // Server running state

    std::vector<int> serverSockets;           // Server socket descriptors
    std::vector<int> clientSockets;           // Connected client socket descriptors
    std::vector<std::thread> listenerThreads; // Threads for each server socket
    std::vector<std::thread> workerThreads;   // Worker threads to process tasks

    std::queue<std::pair<std::vector<uint8_t>, int>> commandQueue; // Queue for client messages
    std::queue<std::function<void()>> taskQueue;                   // Queue for tasks
    std::mutex queueMutex;                                         // Mutex for queue protection
    std::condition_variable queueCondition;                        // Condition variable for command queue
    std::condition_variable taskCondition;                         // Condition variable for task queue
    std::thread commandProcessorThread;                            // Thread to process commands

    std::function<void(const std::vector<uint8_t>&, int)> commandCallback; // Callback for processing commands
    std::atomic<bool> bstop;                                                // Stop flag for worker threads

    void workerThreadFunction(int socket);          // Worker thread function for accepting clients
    void processCommand();                          // Process commands from the queue
    void enqueueTask(std::function<void()> task);   // Enqueue a task for execution
    void workerThread();                            // Task processing worker thread
    void cleanupThreads();                          // Cleanup all threads
};

#endif // UDPSCALABLESERVER_TCPSERVER_H
