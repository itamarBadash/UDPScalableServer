#include "../include/UDPServer.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

UDPServer::UDPServer(int port, int numSockets)
        : port(port), numSockets(numSockets), running(false), bstop(false) {}

UDPServer::~UDPServer() {
    stop();
}

bool UDPServer::start() {
    running = true;

    for (int i = 0; i < numSockets; ++i) {
        int socketFd = socket(AF_INET, SOCK_DGRAM, 0);
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

        struct timeval timeout;
        timeout.tv_sec = 1;  // 1-second timeout
        timeout.tv_usec = 0;
        if (setsockopt(socketFd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
            std::cerr << "Error setting socket timeout: " << strerror(errno) << std::endl;
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

        serverSockets.push_back(socketFd);
        threads.emplace_back(&UDPServer::workerThreadFunction, this, socketFd);
    }

    for (size_t i = 0; i < numSockets; ++i) {
        workerThreads.emplace_back(&UDPServer::workerThread, this);
    }

    commandProcessorThread = std::thread(&UDPServer::processCommand, this);

    std::cout << "UDP Server started with " << numSockets << " sockets on port " << port << std::endl;
    return true;
}

void UDPServer::stop() {
    if (running) {
        std::cout << "Stopping server..." << std::endl;

        running = false;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            bstop = true;
        }
        queueCondition.notify_all();
        taskCondition.notify_all();

        // Join worker threads
        for (auto& thread : threads) {
            if (thread.joinable()) {
                std::cout << "Joining worker thread..." << std::endl;
                thread.join();
            }
        }

        if (commandProcessorThread.joinable()) {
            std::cout << "Joining command processor thread..." << std::endl;
            commandProcessorThread.join();
        }

        for (auto& workerThread : workerThreads) {
            if (workerThread.joinable()) {
                std::cout << "Joining task worker thread..." << std::endl;
                workerThread.join();
            }
        }

        for (int socketFd : serverSockets) {
            std::cout << "Shutting down and closing socket: " << socketFd << std::endl;
            shutdown(socketFd, SHUT_RDWR);  // Interrupt any blocking recvfrom
            close(socketFd);               // Close the socket
        }
        serverSockets.clear();

        std::cout << "Server stopped." << std::endl;
    } else {
        std::cout << "Server is not running." << std::endl;
    }
}

void UDPServer::workerThreadFunction(int socketFd) {
    while (running) {
        sockaddr_in clientAddr{};
        socklen_t clientAddrLen = sizeof(clientAddr);
        std::vector<uint8_t> buffer(1024);

        int bytesReceived = recvfrom(socketFd, buffer.data(), buffer.size(), 0,
                                     (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (bytesReceived < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                // Timeout occurred, check running flag and continue
                continue;
            } else if (errno != EINTR) {  // Ignore interrupts during shutdown
                std::cerr << "Error receiving data: " << strerror(errno) << std::endl;
            }
            continue;
        }

        buffer.resize(bytesReceived);

        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (!isClientRegistered(clientAddr)) {
                clients.push_back(clientAddr);
            }
        }

        {
            std::lock_guard<std::mutex> lock(queueMutex);
            commandQueue.emplace(std::move(buffer), clientAddr);
        }
        queueCondition.notify_one();
    }
}

bool UDPServer::isClientRegistered(const sockaddr_in& clientAddr) {
    for (const auto& client : clients) {
        if (client.sin_addr.s_addr == clientAddr.sin_addr.s_addr &&
            client.sin_port == clientAddr.sin_port) {
            return true;
        }
    }
    return false;
}

void UDPServer::processCommand() {
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

void UDPServer::enqueueTask(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        taskQueue.push(std::move(task));
    }
    taskCondition.notify_one();
}

void UDPServer::workerThread() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            taskCondition.wait(lock, [this] { return bstop || !taskQueue.empty(); });
            if (bstop && taskQueue.empty()) {
                return;  // Exit the thread
            }
            task = std::move(taskQueue.front());
            taskQueue.pop();
        }
        task();
    }
}

void UDPServer::registerCommandCallback(std::function<void(const std::vector<uint8_t>&, const sockaddr_in&)> callback) {
    commandCallback = std::move(callback);
}

void UDPServer::sendToClient(const std::vector<uint8_t>& message, const sockaddr_in& clientAddr) {
    for (int socketFd : serverSockets) {
        sendto(socketFd, message.data(), message.size(), 0, (struct sockaddr*)&clientAddr, sizeof(clientAddr));
    }
}

void UDPServer::sendToAllClients(const std::vector<uint8_t>& message) {
    for (const auto& client : clients) {
        sendToClient(message, client);
    }
}
