# Minimum required CMake version
cmake_minimum_required(VERSION 3.2)

# Project name
project(EMServers VERSION 1.0.0)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Option for building shared libraries
option(BUILD_SHARED_LIBS "Build shared libraries instead of static ones" ON)

# Define the library
add_library(EMServers
        UDPServer.cpp
        TCPServer.cpp
)

# Include the current source directory for public headers
target_include_directories(EMServers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Set library properties for shared builds
if(BUILD_SHARED_LIBS)
    set_target_properties(EMServers PROPERTIES
            VERSION ${PROJECT_VERSION}
            SOVERSION ${PROJECT_VERSION_MAJOR}
    )
    target_compile_definitions(EMServers PRIVATE EM_SERVERS_EXPORTS)
endif()

# Install the library and headers
install(TARGETS EMServers
        EXPORT EMServersTargets                # Export targets for find_package
        LIBRARY DESTINATION lib               # For shared libraries
        ARCHIVE DESTINATION lib               # For static libraries
        INCLUDES DESTINATION include/EMServers
)

install(FILES
        UDPServer.h
        TCPServer.h
        DESTINATION include/EMServers
)

# Generate and install the EMServersConfig.cmake file
install(EXPORT EMServersTargets
        FILE EMServersTargets.cmake
        NAMESPACE EMServers::
        DESTINATION lib/cmake/EMServers
)

configure_file(
        ${CMAKE_SOURCE_DIR}/EMServersConfig.cmake.in
        ${CMAKE_BINARY_DIR}/EMServersConfig.cmake
        @ONLY
)

install(FILES ${CMAKE_BINARY_DIR}/EMServersConfig.cmake
        DESTINATION lib/cmake/EMServers)
