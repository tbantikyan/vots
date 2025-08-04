/*
 * perf_utils.hpp
 * Defines utility method using assembly rdtsc register to read CPU cycle count. Defines associated utility macros.
 */

#pragma once

#include <cstdint>

namespace common {

// Read from the TSC register and return a uint64_t value to represent elapsed CPU clock cycles.
inline auto rdtsc() noexcept {  // NOLINT
    unsigned int lo;
    unsigned int hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return (static_cast<uint64_t>(hi) << 32) | lo;
}
}  // namespace common

// Start latency measurement using rdtsc(). Creates a variable called TAG in the local scope.
#define START_MEASURE(TAG) const auto TAG = common::rdtsc()

// End latency measurement using rdtsc(). Expects a variable called TAG to already exist in the local scope.
// Do while forces user to add ; at the end of macro use.
#define END_MEASURE(TAG, LOGGER)                                                                    \
    do {                                                                                            \
        const auto end = common::rdtsc();                                                           \
        (LOGGER).Log("% RDTSC " #TAG " %\n", common::GetCurrentTimeStr(&time_str_), (end - (TAG))); \
    } while (false)

// Log a current timestamp at the time this macro is invoked.
#define TTT_MEASURE(TAG, LOGGER)                                                        \
    do {                                                                                \
        const auto(TAG) = common::GetCurrentNanos();                                    \
        (LOGGER).Log("% TTT " #TAG " %\n", common::GetCurrentTimeStr(&time_str_), TAG); \
    } while (false)
