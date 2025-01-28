# **EMServers Library**

The **EMServers** library provides a multi-threaded implementation of TCP and UDP servers, enabling efficient handling of concurrent client connections and asynchronous message processing. It is easy to use, extendable, and integrates seamlessly into your C++ projects.

---

## **Features**

- **TCP Server**:

  - Accepts and handles multiple client connections concurrently.
  - Processes client requests in separate threads.
  - Supports clean and graceful shutdown.

- **UDP Server**:

  - Handles multiple sockets for improved throughput.
  - Processes incoming UDP messages asynchronously.
  - Sends responses to individual clients or broadcasts to all clients.

- **Customization**:

  - Register callback functions to handle incoming messages.
  - Thread-safe queuing for tasks and message processing.

---

## **Installation**

### **Requirements**

- **C++17** or later
- **GCC/Clang/Visual Studio** compiler
- **CMake** (for building and integrating the library)

### **Steps**

1. **Clone the Repository**

   ```bash
   git clone https://github.com/your-username/EMServers.git
   cd EMServers
   ```

2. **Build and Install**

   ```bash
   mkdir build
   cd build
   cmake ..
   make
   sudo make install
   ```

   By default, the library and headers will be installed to `/usr/local`. You can change the installation path using CMake:

   ```bash
   cmake .. -DCMAKE_INSTALL_PREFIX=/your/custom/path
   sudo make install
   ```

---

## **Using the Library in Your Project**

### **CMake Integration**

In your `CMakeLists.txt`, add the following to find and link the EMServers library:

```cmake
find_package(EMServers REQUIRED)

add_executable(your_project main.cpp)
target_link_libraries(your_project PRIVATE EMServers::EMServers)
```

### **Include Headers in Your Code**

Include the appropriate headers for the TCP or UDP server:

```cpp
#include <EMServers/TCPServer.h>
#include <EMServers/UDPServer.h>
```

---

## **Usage**

### **1. TCP Server**

#### Initialization

The TCP server listens on a specified port and handles multiple client connections. You can register a callback to process incoming messages.

```cpp
#include <EMServers/TCPServer.h>
#include <iostream>

void handleClientCommand(const std::vector<uint8_t>& message, const sockaddr_in& clientAddr) {
    std::cout << "Received message from client!" << std::endl;
    // Process the message...
}

int main() {
    // Create a TCP server listening on port 8080 with one listener socket
    TCPServer server(8080, 1);

    // Register the command callback
    server.registerCommandCallback(handleClientCommand);

    // Start the server
    if (server.start()) {
        std::cout << "TCP server is running. Press Enter to stop." << std::endl;
        std::cin.get(); // Wait for user input
    }

    // Stop the server
    server.stop();
    return 0;
}
```

### **2. UDP Server**

#### Initialization

The UDP server listens on a specified port and processes incoming messages asynchronously. You can register a callback to handle received messages.

```cpp
#include <EMServers/UDPServer.h>
#include <iostream>

void handleClientCommand(const std::vector<uint8_t>& message, const sockaddr_in& clientAddr) {
    std::cout << "Received message from client!" << std::endl;
    // Process the message...
}

int main() {
    // Create a UDP server listening on port 8080 with two sockets
    UDPServer server(8080, 2);

    // Register the command callback
    server.registerCommandCallback(handleClientCommand);

    // Start the server
    if (server.start()) {
        std::cout << "UDP server is running. Press Enter to stop." << std::endl;
        std::cin.get(); // Wait for user input
    }

    // Stop the server
    server.stop();
    return 0;
}
```

---

## **FAQ**

### How do I change the installation directory?

You can specify a custom installation path by passing `-DCMAKE_INSTALL_PREFIX` to CMake:

```bash
cmake .. -DCMAKE_INSTALL_PREFIX=/custom/path
```

Then, the library will be installed under `/custom/path`.

### How do I link the library manually without CMake?

After installing, link the library using `-lEMServers` and include the headers:

```bash
g++ -std=c++17 -pthread -I/usr/local/include -L/usr/local/lib -lEMServers main.cpp -o main
```

---

## **Contributing**

Contributions are welcome! Feel free to fork the repository, submit pull requests, or open issues.
