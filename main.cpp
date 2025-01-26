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
    char clientIp[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, INET_ADDRSTRLEN);

    std::cout << "nigga " << clientIp << ":" << ntohs(clientAddr.sin_port)
              << " - " << strMessage << std::endl;

    // Echo the message back to the client
    std::vector<uint8_t> response(message);
    response.insert(response.end(), {' ', '-', ' ', 'e', 'c', 'h', 'o', 'e', 'd'});
    serverPtr->sendToAllClients(response); // Broadcast the response
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