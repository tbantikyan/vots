cmake_minimum_required(VERSION 3.10)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) 
set(BUILD_SHARED_LIBS OFF) 
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON) 
set(CMAKE_VERBOSE_MAKEFILE on)

project(vots
        VERSION 0 
        DESCRIPTION "Very optimized trading system"
        LANGUAGES C CXX
)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        message(STATUS "Setting build type to `Debug` as none was specified.")
        set(CMAKE_BUILD_TYPE "Debug")
endif()

# This checks if you're running CMake from a folder that already has CMakeLists.txt.
# Importantly, this catches the common case of running it from the root directory.
file(TO_CMAKE_PATH "${PROJECT_BINARY_DIR}/CMakeLists.txt" PATH_TO_CMAKELISTS_TXT)

if(EXISTS "${PATH_TO_CMAKELISTS_TXT}")
        message(FATAL_ERROR "Run CMake from a build subdirectory! \"mkdir build ; cd build ; cmake ..\" \
    Some junk files were created in this folder (CMakeCache.txt, CMakeFiles); you should delete those.")
endif()

set(VOTS_CLANG_SEARCH_PATH "/usr/local/bin" "/usr/bin" "/usr/local/opt/llvm/bin" "/usr/local/opt/llvm@14/bin"
        "/opt/homebrew/opt/llvm@14/bin/")

# CTest
enable_testing()

set(CLANG_SEARCH_PATH "/usr/local/bin" "/usr/bin" "/usr/local/opt/llvm/bin" "/usr/local/opt/llvm@14/bin"
        "/opt/homebrew/opt/llvm@14/bin/")

# attempt to find the binary if user did not specify
find_program(CLANG_FORMAT_BIN
        NAMES clang-format clang-format-14
        HINTS ${CLANG_SEARCH_PATH})

if("${CLANG_FORMAT_BIN}" STREQUAL "CLANG_FORMAT_BIN-NOTFOUND")
        message(WARNING "couldn't find clang-format.")
else()
        message(STATUS "found clang-format at ${CLANG_FORMAT_BIN}")
endif()

# attempt to find the binary if user did not specify
find_program(CLANG_TIDY_BIN
        NAMES clang-tidy clang-tidy-14
        HINTS ${CLANG_SEARCH_PATH})

if("${CLANG_TIDY_BIN}" STREQUAL "CLANG_TIDY_BIN-NOTFOUND")
        message(WARNING "couldn't find clang-tidy.")
else()
        # Output compile_commands.json
        set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
        message(STATUS "found clang-tidy at ${CLANG_TIDY_BIN}")
endif()

# COMPILER SETUP
if(NOT DEFINED VOTS_SANITIZER)
    set(VOTS_SANITIZER address)
endif()

message("Build mode: ${CMAKE_BUILD_TYPE}")
message("${VOTS_SANITIZER} sanitizer will be enabled in debug mode.")

# Compiler flags.
set(CMAKE_CXX_FLAGS "-std=c++2a -Wall -Wextra -Werror -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -ggdb -fsanitize=${VOTS_SANITIZER} -fno-omit-frame-pointer -fno-optimize-sibling-calls")

message(STATUS "CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
message(STATUS "CMAKE_CXX_FLAGS_DEBUG: ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "CMAKE_EXE_LINKER_FLAGS: ${CMAKE_EXE_LINKER_FLAGS}")
message(STATUS "CMAKE_SHARED_LINKER_FLAGS: ${CMAKE_SHARED_LINKER_FLAGS}")

# Output directory.
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Third party
find_package(GTest REQUIRED)

# Includes.
set(VOTS_SRC_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/src/include)
set(VOTS_TEST_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/test/include)

include_directories(${VOTS_SRC_INCLUDE_DIR} ${VOTS_TEST_INCLUDE_DIR})
include_directories(BEFORE src) # This is needed for gtest.

function(disable_target_warnings NAME)
        target_compile_options(${NAME} PRIVATE "-w")
endfunction()

# Other CMake modules
add_subdirectory(src)
add_subdirectory(test)

# Custom make targets
add_custom_target(lint
    COMMAND ${CMAKE_SOURCE_DIR}/run_clang_tidy.py 
        -clang-tidy-binary ${CLANG_TIDY_BIN} 
        -p ${CMAKE_BINARY_DIR} 
        ${CMAKE_SOURCE_DIR}/src 
        ${CMAKE_SOURCE_DIR}/test
)
add_dependencies(lint GTest::gtest vots) 

add_custom_target(lint-fix
        ${CMAKE_SOURCE_DIR}/run_clang_tidy.py 
        -clang-tidy-binary ${CLANG_TIDY_BIN} 
        -p ${CMAKE_BINARY_DIR} 
        -fix 
        ${CMAKE_SOURCE_DIR}/src 
        ${CMAKE_SOURCE_DIR}/test
)

string(CONCAT VOTS_FORMAT_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/src,"
        "${CMAKE_CURRENT_SOURCE_DIR}/test,"
)

add_custom_target(format ${CMAKE_SOURCE_DIR}/run_clang_format.py
        ${CLANG_FORMAT_BIN}
        ${CMAKE_SOURCE_DIR}/format_exclusions.txt
        --source_dirs
        ${VOTS_FORMAT_DIRS}
        --quiet
)

add_custom_target(format-fix ${CMAKE_SOURCE_DIR}/run_clang_format.py
        ${CLANG_FORMAT_BIN}
        ${CMAKE_SOURCE_DIR}/format_exclusions.txt
        --source_dirs
        ${VOTS_FORMAT_DIRS}
        --fix
        --quiet
)

# main executables
add_executable(exchange_main src/exchange_main.cpp)

target_link_libraries(exchange_main PRIVATE vots)

target_include_directories(exchange_main PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
