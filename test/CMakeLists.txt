cmake_minimum_required(VERSION 3.10)

include(GoogleTest)

file(GLOB_RECURSE VOTS_TEST_SOURCES "${PROJECT_SOURCE_DIR}/test/*/*test.cpp")

# "make XYZ_test"
foreach (vots_test_source ${VOTS_TEST_SOURCES})
    # Create a human readable name.
    get_filename_component(vots_test_filename ${vots_test_source} NAME)
    string(REPLACE ".cpp" "" vots_test_name ${vots_test_filename})

    # Add the test target separately and as part of "make check-tests".
    add_executable(${vots_test_name} EXCLUDE_FROM_ALL ${vots_test_source})

    gtest_discover_tests(${vots_test_name}
            EXTRA_ARGS
            --gtest_output=xml:${CMAKE_BINARY_DIR}/test/${vots_test_name}.xml
            --gtest_catch_exceptions=0
            DISCOVERY_TIMEOUT 120
            PROPERTIES
            TIMEOUT 120
            )

    target_link_libraries(${vots_test_name} vots GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main)

    # Set test target properties and dependencies.
    set_target_properties(${vots_test_name}
            PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test"
            COMMAND ${vots_test_name}
    )

    list(APPEND ALL_VOTS_TEST_TARGETS ${vots_test_name})
endforeach ()

# Set catch-all test run
add_custom_target(
  build-tests
  DEPENDS ${ALL_VOTS_TEST_TARGETS}
)
