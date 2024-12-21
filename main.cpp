#include <iostream>
#include <csignal>
#include "UDPServer.h"
//a
UDPServer* serverPtr = nullptr;

void signalHandler(int signum) {
    if (serverPtr) {
        serverPtr->stop();
    }
    exit(signum);
}

int main() {
    UDPServer server(8080, 4);
    serverPtr = &server;

    // Register signal handler
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    server.start();

    // Keep the main thread running to handle signals
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}