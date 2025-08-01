add_subdirectory(matching_engine)
add_subdirectory(network)
add_subdirectory(order_server)

add_library(vots STATIC ${ALL_OBJECT_FILES})

set(VOTS_LIBS
        vots_matching_engine
        vots_network
        vots_order_server
        )

set(VOTS_THIRDPARTY_LIBS
        )

target_link_libraries(
        vots 
        ${VOTS_LIBS}
        ${VOTS_THIRDPARTY_LIBS}
    )

target_include_directories(
        vots PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
