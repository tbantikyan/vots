add_library(
        vots_order_server
        OBJECT
        order_server.cpp
    )

target_link_libraries(
        vots_order_server
        # lib
    )

set(ALL_OBJECT_FILES
        ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:vots_order_server>
        PARENT_SCOPE)

