add_subdirectory(network)

add_library(vots STATIC ${ALL_OBJECT_FILES})

set(VOTS_LIBS
        vots_network
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
