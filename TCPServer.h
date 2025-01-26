#ifndef UDPSCALABLESERVER_TCPSERVER_H
#define UDPSCALABLESERVER_TCPSERVER_H

#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <functional>
#include <netinet/in.h>

class TCPServer {
public:
    TCPServer(int port, int numSockets);
    ~TCPServer();

    bool start();
    void stop();
    bool send_message(const std::string& message);
    bool send_message_to_client(const std::string &message, int clientSocket);
    void registerCommandCallback(std::function<void(const std::string&)> callback);

private:
    void setupServerAddress(sockaddr_in& addr, int port);
    void acceptConnections(int serverSocket);
    void handleClient(int clientSocket);
    void processCommands();
    void enqueueTask(std::function<void()> task);
    void workerThread();
    void cleanupThreads();

    int port;
    int numSockets;
    std::vector<int> serverSockets;
    std::vector<std::thread> acceptThreads;
    bool running;

    std::vector<int> clientSockets;
    std::vector<std::thread> clientThreads;
    std::thread commandProcessorThread;
    std::vector<std::thread> workerThreads;

    std::mutex clientSocketsMutex;
    std::mutex queueMutex;
    std::queue<std::string> commandQueue;
    std::queue<std::function<void()>> taskQueue;
    std::condition_variable queueCondition;
    std::condition_variable taskCondition;

    std::function<void(const std::string&)> commandCallback;
    bool bstop;
};

#endif //UDPSCALABLESERVER_TCPSERVER_H