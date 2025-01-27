cmake_minimum_required(VERSION 3.2)

project(EMServers VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_SHARED_LIBS "Build shared libraries instead of static ones" ON)

add_library(EMServers
        UDPServer.cpp
        TCPServer.cpp
)

target_include_directories(EMServers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(BUILD_SHARED_LIBS)
    set_target_properties(EMServers PROPERTIES
            VERSION ${PROJECT_VERSION}
            SOVERSION ${PROJECT_VERSION_MAJOR}
    )
endif()

if(BUILD_SHARED_LIBS)
    target_compile_definitions(EMServers PRIVATE EM_SERVERS_EXPORTS)
endif()

install(TARGETS EMServers
        LIBRARY DESTINATION lib        # For shared libraries
        ARCHIVE DESTINATION lib        # For static libraries
)

install(FILES
        UDPServer.h
        TCPServer.h
        DESTINATION include/EMServers
)
