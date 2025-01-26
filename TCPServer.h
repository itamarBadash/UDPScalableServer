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
    TCPServer(int port);
    ~TCPServer();

    bool start();
    void stop();
    bool send_message(const std::string& message);
    bool send_message_to_client(const std::string &message, int clientSocket);

private:
    void setupServerAddress();
    void acceptConnections();
    void handleClient(int clientSocket);
    void processCommands();
    void cleanupThreads();

    int port;
    int serverSocket;
    sockaddr_in serverAddr;
    bool running;

    std::vector<int> clientSockets;
    std::vector<std::thread> clientThreads;
    std::thread commandProcessorThread;

    std::mutex clientSocketsMutex;
    std::mutex queueMutex;
    std::queue<std::string> commandQueue;
    std::condition_variable queueCondition;

};


#endif //UDPSCALABLESERVER_TCPSERVER_H
