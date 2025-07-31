add_library(
        vots_network
        OBJECT
        mcast_socket.cpp
        tcp_socket.cpp
        tcp_server.cpp
    )

target_link_libraries(
        vots_network
        # lib
    )

set(ALL_OBJECT_FILES
        ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:vots_network>
        PARENT_SCOPE)
