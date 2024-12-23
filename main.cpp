#include <iostream>
#include <csignal>
#include <arpa/inet.h>

#include "UDPServer.h"

UDPServer* serverPtr = nullptr;

void signalHandler(int signum) {
    if (serverPtr) {
        serverPtr->stop();
    }
    exit(signum);
}

void handleCommand(const std::string& message, const sockaddr_in& clientAddr) {
    std::cout << "Custom handler: " << message << std::endl;
}

int main() {
    UDPServer server(8080, 4);
    serverPtr = &server;

    // Register signal handler
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Register command callback
    server.registerCommandCallback(handleCommand);

    server.start();

    // Keep the main thread running to handle signals
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}