/*
 * integrity.hpp
 * Utility functions that can operate of and on the flow of the program.
 */

#pragma once

#include <cstdlib>
#include <iostream>
#include <string>

inline auto ASSERT(bool cond, const std::string &msg) noexcept {
    if (!cond) [[unlikely]] {
        std::cerr << msg << '\n';
        exit(EXIT_FAILURE);
    }
}

inline auto FATAL(const std::string &msg) noexcept {
    std::cerr << msg << '\n';
    exit(EXIT_FAILURE);
}
