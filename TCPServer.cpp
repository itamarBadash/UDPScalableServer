#include "TCPServer.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <algorithm>

TCPServer::TCPServer(int port, int numSockets) : port(port), numSockets(numSockets), running(false), bstop(false) {}

TCPServer::~TCPServer() {
    stop();
    cleanupThreads();
}

void TCPServer::setupServerAddress(sockaddr_in& addr, int port) {
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
}

bool TCPServer::start() {
    for (int i = 0; i < numSockets; ++i) {
        int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket < 0) {
            std::cerr << "Error creating socket: " << strerror(errno) << std::endl;
            return false;
        }

        int opt = 1;
        if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
            std::cerr << "Error setting SO_REUSEPORT: " << strerror(errno) << std::endl;
            close(serverSocket);
            return false;
        }

        sockaddr_in serverAddr;
        setupServerAddress(serverAddr, port + i);

        if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            std::cerr << "Error binding socket: " << strerror(errno) << std::endl;
            close(serverSocket);
            return false;
        }

        if (listen(serverSocket, 10) < 0) {
            std::cerr << "Error listening on socket: " << strerror(errno) << std::endl;
            close(serverSocket);
            return false;
        }

        serverSockets.push_back(serverSocket);
        acceptThreads.emplace_back(&TCPServer::acceptConnections, this, serverSocket);
    }

    running = true;
    std::cout << "Server started with " << numSockets << " sockets on port " << port << std::endl;

    commandProcessorThread = std::thread(&TCPServer::processCommand, this);
    for (size_t i = 0; i < numSockets; ++i) {
        workerThreads.emplace_back(&TCPServer::workerThread, this);
    }

    return true;
}

void TCPServer::stop() {
    if (running) {
        running = false;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            bstop = true;
        }
        queueCondition.notify_all();
        taskCondition.notify_all();

        for (int serverSocket : serverSockets) {
            close(serverSocket);
        }
        std::cout << "Server stopped." << std::endl;
        if (commandProcessorThread.joinable()) {
            commandProcessorThread.join();
        }
    }
}

void TCPServer::acceptConnections(int serverSocket) {
    while (running) {
        int clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket < 0) {
            if (running) {
                std::cerr << "Error accepting connection: " << strerror(errno) << std::endl;
            }
            continue;
        }

        std::cout << "Client connected." << std::endl;
        {
            std::lock_guard<std::mutex> lock(clientSocketsMutex);
            clientSockets.push_back(clientSocket);
        }
        clientThreads.emplace_back(&TCPServer::handleClient, this, clientSocket);
    }
}

void TCPServer::handleClient(int clientSocket) {
    const int bufferSize = 1024;
    char buffer[bufferSize];

    while (running) {
        int bytesReceived = recv(clientSocket, buffer, bufferSize - 1, 0);
        if (bytesReceived < 0) {
            std::cerr << "Error receiving data: " << strerror(errno) << std::endl;
            break;
        } else if (bytesReceived == 0) {
            std::cout << "Client disconnected." << std::endl;
            break;
        }

        buffer[bytesReceived] = '\0';
        std::vector<uint8_t> message(buffer, buffer + bytesReceived);

        sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        getpeername(clientSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);

        {
            std::lock_guard<std::mutex> lock(queueMutex);
            commandQueue.emplace(std::move(message), clientAddr);
        }
        queueCondition.notify_one();
    }

    close(clientSocket);
    {
        std::lock_guard<std::mutex> lock(clientSocketsMutex);
        clientSockets.erase(std::remove(clientSockets.begin(), clientSockets.end(), clientSocket), clientSockets.end());
    }
}

void TCPServer::processCommand() {
    while (running) {
        std::cout<<"aaa"<<std::endl;
        std::unique_lock<std::mutex> lock(queueMutex);
        queueCondition.wait(lock, [this] { return !commandQueue.empty() || !running; });

        while (!commandQueue.empty()) {
            auto [message, clientAddr] = std::move(commandQueue.front());
            commandQueue.pop();
            lock.unlock();

            enqueueTask([message = std::move(message), clientAddr, this] {
                if (commandCallback) {
                    commandCallback(message, clientAddr);
                }
            });

            lock.lock();
        }
    }
}

void TCPServer::enqueueTask(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        taskQueue.push(std::move(task));
    }
    taskCondition.notify_one();
}

void TCPServer::workerThread() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            taskCondition.wait(lock, [this] { return bstop || !taskQueue.empty(); });
            if (bstop && taskQueue.empty()) {
                return;
            }
            task = std::move(taskQueue.front());
            taskQueue.pop();
        }
        task();
    }
}

void TCPServer::registerCommandCallback(std::function<void(const std::vector<uint8_t>&, const sockaddr_in&)> callback) {
    commandCallback = std::move(callback);
}

void TCPServer::cleanupThreads() {
    for (auto& thread : clientThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    clientThreads.clear();

    for (auto& thread : acceptThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    acceptThreads.clear();

    for (auto& thread : workerThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    workerThreads.clear();
}

bool TCPServer::send_message(const std::string& message) {
    std::lock_guard<std::mutex> lock(clientSocketsMutex);
    if (clientSockets.empty()) {
        std::cerr << "No clients connected" << std::endl;
        return false;
    }

    size_t totalBytesSent = 0;
    size_t messageLength = message.size();
    const char* messagePtr = message.c_str();

    for (int clientSocket : clientSockets) {
        totalBytesSent = 0;
        while (totalBytesSent < messageLength) {
            ssize_t bytesSent = send(clientSocket, messagePtr + totalBytesSent, messageLength - totalBytesSent, 0);
            if (bytesSent < 0) {
                std::cerr << "Failed to send message to client. Error: " << strerror(errno) << std::endl;
                break;
            }
            totalBytesSent += bytesSent;
        }
    }

    return true;
}

bool TCPServer::send_message_to_client(const std::string& message, int clientSocket) {
    size_t totalBytesSent = 0;
    size_t messageLength = message.size();
    const char* messagePtr = message.c_str();

    while (totalBytesSent < messageLength) {
        ssize_t bytesSent = send(clientSocket, messagePtr + totalBytesSent, messageLength - totalBytesSent, 0);
        if (bytesSent < 0) {
            std::cerr << "Failed to send message to client. Error: " << strerror(errno) << std::endl;
            return false;
        }
        totalBytesSent += bytesSent;
    }

    return true;
}