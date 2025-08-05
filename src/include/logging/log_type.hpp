/*
 * log_type.hpp
 * Enumeration indicating the type of the data being logged.
 */

#pragma once

#include <cstdint>

namespace common {

enum class LogType : int8_t {
    CHAR = 0,
    INTEGER = 1,
    LONG_INTEGER = 2,
    LONG_LONG_INTEGER = 3,
    UNSIGNED_INTEGER = 4,
    UNSIGNED_LONG_INTEGER = 5,
    UNSIGNED_LONG_LONG_INTEGER = 6,
    FLOAT = 7,
    DOUBLE = 8,
    STRING = 9
};

}  // namespace common
