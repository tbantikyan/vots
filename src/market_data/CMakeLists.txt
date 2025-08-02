add_library(
        vots_market_data
        OBJECT
        market_data_publisher.cpp
        snapshot_synthesizer.cpp
    )

target_link_libraries(
        vots_market_data
        # lib
    )

set(ALL_OBJECT_FILES
        ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:vots_market_data>
        PARENT_SCOPE)

