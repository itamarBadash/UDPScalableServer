#include "TCPServer.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>

TCPServer::TCPServer(int port, int numSockets)
        : port(port), numSockets(numSockets), running(false), bstop(false) {}

TCPServer::~TCPServer() {
    stop();
}

bool TCPServer::start() {
    running = true;

    for (int i = 0; i < numSockets; ++i) {
        int socketFd = socket(AF_INET, SOCK_STREAM, 0);
        if (socketFd < 0) {
            std::cerr << "Error creating socket: " << strerror(errno) << std::endl;
            return false;
        }

        int opt = 1;
        if (setsockopt(socketFd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
            std::cerr << "Error setting SO_REUSEPORT: " << strerror(errno) << std::endl;
            close(socketFd);
            return false;
        }

        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port);

        if (bind(socketFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            std::cerr << "Error binding socket: " << strerror(errno) << std::endl;
            close(socketFd);
            return false;
        }

        if (listen(socketFd, 10) < 0) {
            std::cerr << "Error listening on socket: " << strerror(errno) << std::endl;
            close(socketFd);
            return false;
        }

        serverSockets.push_back(socketFd);
        listenerThreads.emplace_back(&TCPServer::workerThreadFunction, this, socketFd);
    }

    for (size_t i = 0; i < numSockets; ++i) {
        workerThreads.emplace_back(&TCPServer::workerThread, this);
    }
    commandProcessorThread = std::thread(&TCPServer::processCommand, this);

    std::cout << "TCP Server started with " << numSockets << " sockets on port " << port << std::endl;
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

        for (auto& thread : listenerThreads) {
            if (thread.joinable()) {
                thread.join();
            }
        }

        for (auto& workerThread : workerThreads) {
            if (workerThread.joinable()) {
                workerThread.join();
            }
        }

        for (int socketFd : serverSockets) {
            close(socketFd);
        }

        if (commandProcessorThread.joinable()) {
            commandProcessorThread.join();
        }

        serverSockets.clear();
    }
    std::cout << "Server stopped." << std::endl;
}

void TCPServer::workerThreadFunction(int serverSocket) {
    while (running) {
        sockaddr_in clientAddr{};
        socklen_t clientAddrLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket < 0) {
            if (running) {
                std::cerr << "Error accepting connection: " << strerror(errno) << std::endl;
            }
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(queueMutex);
            clientSockets.push_back(clientSocket);
        }

        std::vector<uint8_t> buffer(1024);
        int bytesReceived = recv(clientSocket, buffer.data(), buffer.size(), 0);
        if (bytesReceived > 0) {
            buffer.resize(bytesReceived);

            {
                std::lock_guard<std::mutex> lock(queueMutex);
                commandQueue.emplace(std::move(buffer), clientAddr);
            }
            queueCondition.notify_one();
        } else {
            close(clientSocket);
        }
    }
}

void TCPServer::processCommand() {
    while (running) {
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
        std::lock_guard<std::mutex> lock(queueMutex);
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

void TCPServer::sendToClient(const std::vector<uint8_t>& message, int clientSocket) {
    send(clientSocket, message.data(), message.size(), 0);
}

void TCPServer::sendToAllClients(const std::vector<uint8_t>& message) {
    std::lock_guard<std::mutex> lock(queueMutex);
    for (int clientSocket : clientSockets) {
        sendToClient(message, clientSocket);
    }
}
