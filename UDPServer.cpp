#include "UDPServer.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

UDPServer::UDPServer(int port, int numSockets)
        : port(port), numSockets(numSockets), running(false) {}

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
    commandProcessorThread = std::thread(&UDPServer::processCommand, this);
    std::cout << "UDP Server started with " << numSockets << " sockets on port " << port << std::endl;
    return true;
}

void UDPServer::stop() {
    if (running) {
        running = false;
        for (auto& thread : threads) {
            if (thread.joinable()) {
                thread.join();
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

void UDPServer::workerThreadFunction(int socketFd) {
    while (running) {
        sockaddr_in clientAddr{};
        socklen_t clientAddrLen = sizeof(clientAddr);
        char buffer[1024];

        int bytesReceived = recvfrom(socketFd, buffer, sizeof(buffer) - 1, 0,
                                     (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (bytesReceived < 0) {
            if (errno != EWOULDBLOCK && errno != EAGAIN) {
                std::cerr << "Error receiving data: " << strerror(errno) << std::endl;
            }
            continue;
        }

        buffer[bytesReceived] = '\0';
        std::string message(buffer);

        // Add the message to the command queue
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            commandQueue.emplace(message, clientAddr);
        }
        queueCondition.notify_one();
    }
}

void UDPServer::processCommand() {
    while (running) {
        std::unique_lock<std::mutex> lock(queueMutex);
        queueCondition.wait(lock, [this] { return !commandQueue.empty() || !running; });

        while (!commandQueue.empty()) {
            auto [message, clientAddr] = commandQueue.front();
            commandQueue.pop();
            lock.unlock();

            size_t pos = message.find(':');
            if (pos != std::string::npos) {
                std::string command = message.substr(0, pos);
                std::string params_str = message.substr(pos + 1);
                std::vector<float> params;

                size_t start = 0;
                size_t end;
                while ((end = params_str.find(',', start)) != std::string::npos) {
                    try {
                        params.push_back(std::stof(params_str.substr(start, end - start)));
                    } catch (const std::exception &e) {
                        std::cerr << "Error parsing parameter: " << e.what() << std::endl;
                    }
                    start = end + 1;
                }

                if (start < params_str.length()) {
                    try {
                        params.push_back(std::stof(params_str.substr(start)));
                    } catch (const std::exception &e) {
                        std::cerr << "Error parsing parameter: " << e.what() << std::endl;
                    }
                }
                std::cout << "Received command: " << command << "Parameters: " << params_str << std::endl;
                lock.lock();
            }
        }
    }
}
