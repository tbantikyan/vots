/*
 * perf_utils.hpp
 * Defines utility method using an assembly register to read CPU cycle count. Defines associated utility macros.
 */

#pragma once

#include <cstdint>

namespace common {

// Read from the TSC register and return a uint64_t value to represent elapsed CPU clock cycles.
inline auto CycleCount() noexcept {
    uint64_t cnt;
    asm volatile("mrs %0, CNTVCT_EL0" : "=r"(cnt));
    return cnt;
}
}  // namespace common

// Start latency measurement using CycleCount(). Creates a variable called TAG in the local scope.
#define START_MEASURE(TAG) const auto TAG = common::CycleCount()

// End latency measurement using CycleCount(). Expects a variable called TAG to already exist in the local scope.
// Do while forces user to add ; at the end of macro use.
#define END_MEASURE(TAG, LOGGER)                                                                          \
    do {                                                                                                  \
        const auto end = common::CycleCount();                                                            \
        (LOGGER).Log("% Cycle Count " #TAG " %\n", common::GetCurrentTimeStr(&time_str_), (end - (TAG))); \
    } while (false)

// Log a current timestamp at the time this macro is invoked.
#define TTT_MEASURE(TAG, LOGGER)                                                        \
    do {                                                                                \
        const auto TAG = common::GetCurrentNanos();                                     \
        (LOGGER).Log("% TTT " #TAG " %\n", common::GetCurrentTimeStr(&time_str_), TAG); \
    } while (false)
