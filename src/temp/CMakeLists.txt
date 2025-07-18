add_library(
        vots_temp
        OBJECT
        main.cpp
    )

target_link_libraries(
        vots_temp
        # lib
    )

set(ALL_OBJECT_FILES
        ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:vots_temp>
        PARENT_SCOPE)

