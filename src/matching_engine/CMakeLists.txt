add_library(
        vots_matching_engine
        OBJECT
        order_book.cpp
    )

target_link_libraries(
        vots_matching_engine
        # lib
    )

set(ALL_OBJECT_FILES
        ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:vots_matching_engine>
        PARENT_SCOPE)

