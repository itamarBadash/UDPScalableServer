#include <iostream>
#include "UDPServer.h"
int main() {
    UDPServer server(8080, 4);

    server.start();
    return 0;
}
