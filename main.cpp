#include <iostream>
#include <csignal>
#include <arpa/inet.h>
#include <vector>

#include "UDPServer.h"
#include "TCPServer.h"

UDPServer* serverPtr = nullptr;

void signalHandler(int signum) {
    if (serverPtr) {
        serverPtr->stop();
    }
    exit(signum);
}

void handleCommand(const std::vector<uint8_t>& message, const sockaddr_in& clientAddr) {
    std::string strMessage(message.begin(), message.end());
    std::cout << "Custom handler: " << strMessage << std::endl;
}

int main() {
    TCPServer server(8080, 4);
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