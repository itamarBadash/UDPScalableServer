#include "../include/TCPServer.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <algorithm>

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
        std::cout << "Socket " << i + 1 << " listening on port " << port << std::endl;
    }

    commandProcessorThread = std::thread(&TCPServer::processCommand, this);
    std::cout << "TCP Server started with " << numSockets << " sockets on port " << port << std::endl;
    return true;
}

void TCPServer::stop() {
    if (running) {
        std::cout << "Stopping server..." << std::endl;

        running = false;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            bstop = true;
        }
        queueCondition.notify_all();
        taskCondition.notify_all();

        for (int socketFd : serverSockets) {
            shutdown(socketFd, SHUT_RDWR);  // Interrupt accept
            close(socketFd);
        }

        for (auto& thread : listenerThreads) {
            if (thread.joinable()) {
                thread.join();
            }
        }

        if (commandProcessorThread.joinable()) {
            commandProcessorThread.join();
        }

        for (auto& thread : clientThreads) {
            if (thread.joinable()) {
                thread.join();
            }
        }

        {
            std::lock_guard<std::mutex> lock(queueMutex);
            for (int clientSocket : clientSockets) {
                shutdown(clientSocket, SHUT_RDWR);
                close(clientSocket);
            }
            clientSockets.clear();
            clientAddresses.clear();
        }

        serverSockets.clear();
        std::cout << "Server stopped." << std::endl;
    }
}

void TCPServer::workerThreadFunction(int serverSocket) {
    while (running) {
        sockaddr_in clientAddr{};
        socklen_t clientAddrLen = sizeof(clientAddr);

        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket < 0) {
            if (errno == EINTR) {
                break;
            } else if (running) {
                std::cerr << "Error accepting connection: " << strerror(errno) << std::endl;
            }
            continue;
        }

        char clientIp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, INET_ADDRSTRLEN);
        std::cout << "Client connected: " << clientIp << ":" << ntohs(clientAddr.sin_port) << std::endl;

        {
            std::lock_guard<std::mutex> lock(queueMutex);
            clientSockets.push_back(clientSocket);
            clientAddresses[clientSocket] = clientAddr; // Save client address
        }

        clientThreads.emplace_back(&TCPServer::handleClient, this, clientSocket);
    }
}

void TCPServer::handleClient(int clientSocket) {
    const size_t bufferSize = 1024;
    std::vector<uint8_t> buffer(bufferSize);

    while (running) {
        int bytesReceived = recv(clientSocket, buffer.data(), buffer.size(), 0);
        if (bytesReceived > 0) {
            buffer.resize(bytesReceived);

            sockaddr_in clientAddr;
            {
                std::lock_guard<std::mutex> lock(queueMutex);
                clientAddr = clientAddresses[clientSocket];
            }

            {
                std::lock_guard<std::mutex> lock(queueMutex);
                commandQueue.emplace(std::move(buffer), clientAddr);
            }
            queueCondition.notify_one();
        } else if (bytesReceived == 0) {
            std::cout << "Client disconnected: socket " << clientSocket << std::endl;
            break; // Graceful disconnect
        } else if (errno == EINTR) {
            break; // Interrupted during shutdown
        } else {
            std::cerr << "Error receiving data from socket " << clientSocket
                      << ": " << strerror(errno) << std::endl;
            break;
        }

        buffer.resize(bufferSize); // Reset buffer size for the next read
    }

    close(clientSocket);
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        clientSockets.erase(std::remove(clientSockets.begin(), clientSockets.end(), clientSocket), clientSockets.end());
        clientAddresses.erase(clientSocket);
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

            if (commandCallback) {
                commandCallback(message, clientAddr);
            }

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

void TCPServer::registerCommandCallback(std::function<void(const std::vector<uint8_t>&, const sockaddr_in&)> callback) {
    commandCallback = std::move(callback);
}

void TCPServer::sendToClient(const std::vector<uint8_t>& message, int clientSocket) {
    if (message.empty()) {
        std::cerr << "Cannot send an empty message to client." << std::endl;
        return;
    }

    ssize_t bytesSent = send(clientSocket, message.data(), message.size(), 0);
    if (bytesSent < 0) {
        std::cerr << "Error sending message to client socket " << clientSocket
                  << ": " << strerror(errno) << std::endl;
    }
}

void TCPServer::sendToAllClients(const std::vector<uint8_t>& message) {
    if (message.empty()) {
        std::cerr << "Cannot send an empty message to all clients." << std::endl;
        return;
    }

    std::lock_guard<std::mutex> lock(queueMutex); // Protect access to clientSockets
    for (int clientSocket : clientSockets) {
        ssize_t bytesSent = send(clientSocket, message.data(), message.size(), 0);
        if (bytesSent < 0) {
            std::cerr << "Error sending message to client socket " << clientSocket
                      << ": " << strerror(errno) << std::endl;
        }
    }
}
