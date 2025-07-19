/*
 * time_utils.hpp
 * Provides time utilities for use in logging
 */

#pragma once

#include <chrono>
#include <ctime>
#include <string>

namespace common {

using Nanos = int64_t;

constexpr Nanos NANOS_TO_MICROS = 1000;
constexpr Nanos MICROS_TO_MILLIS = 1000;
constexpr Nanos MILLIS_TO_SECS = 1000;
constexpr Nanos NANOS_TO_MILLIS = NANOS_TO_MICROS * MICROS_TO_MILLIS;
constexpr Nanos NANOS_TO_SECS = NANOS_TO_MILLIS * MILLIS_TO_SECS;

inline auto GetCurrentNanos() noexcept {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}

inline auto GetCurrentTimeStr(std::string *time_str) -> auto & {
    using std::chrono::system_clock;
    const auto time = system_clock::to_time_t(system_clock::now());
    time_str->assign(ctime(&time));
    if (!time_str->empty()) {
        time_str->at(time_str->length() - 1) = '\0';  // replace new line character with null terminator
    }
    return *time_str;
}

}  // namespace common
