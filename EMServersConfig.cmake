set(EMServers_INCLUDE_DIR "${CMAKE_INSTALL_PREFIX}/include/EMServers")

set(EMServers_LIBRARIES "${CMAKE_INSTALL_PREFIX}/lib/libEMServers.so")

include_directories(${EMServers_INCLUDE_DIR})

add_library(EMServers SHARED IMPORTED)
set_target_properties(EMServers PROPERTIES
        IMPORTED_LOCATION ${EMServers_LIBRARIES}
        INTERFACE_INCLUDE_DIRECTORIES ${EMServers_INCLUDE_DIR}
)

set(EMServers_FOUND TRUE)
