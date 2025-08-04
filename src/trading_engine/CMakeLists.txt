add_library(
        vots_trading_engine
        OBJECT
        order_manager.cpp
        trading_engine.cpp
    )

target_link_libraries(
        vots_trading_engine
        # lib
    )

set(ALL_OBJECT_FILES
        ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:vots_trading_engine>
        PARENT_SCOPE)


