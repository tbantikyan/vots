add_library(
        vots_order_gateway
        OBJECT
        order_server.cpp
    )

target_link_libraries(
        vots_order_gateway
        # lib
    )

set(ALL_OBJECT_FILES
        ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:vots_order_gateway>
        PARENT_SCOPE)

