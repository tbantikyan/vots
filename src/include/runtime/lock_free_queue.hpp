/*
 * lock_free_queue.hpp
 * Provides implementation of a generic, fixed-sized, lock-free queue. Provides safe concurrent data sharing for the
 * Single Producer Single Consumer paradigm.
 */

#pragma once

#include <pthread.h>

#include <atomic>
#include <cstddef>
#include <string>
#include <vector>

#include "integrity/integrity.hpp"

namespace common {

/*
 * Single Producer Single Consumer, fixed-sized, lock-free queue.
 *
 * The queue implements two-phase write and read operations. This enables partial writes, minimizing atomic operations
 * and optimizing memory usage. Moreover, it simplifies shared data management, as consumption and production is only
 * visible after "commits" done via the Update* methods.
 */
template <typename T>
class LockFreeQueue final {
   public:
    explicit LockFreeQueue(std::size_t num_elems) : store_(num_elems, T()) {}

    auto GetNextToWriteTo() noexcept { return &store_[next_write_index_]; }

    auto UpdateWriteIndex() noexcept {
        next_write_index_ = (next_write_index_ + 1) % store_.size();
        num_elements_++;
    }

    auto GetNextToRead() const noexcept -> const T * { return (Size() ? &store_[next_read_index_] : nullptr); }

    auto UpdateReadIndex() noexcept {
        next_read_index_ = (next_read_index_ + 1) % store_.size();
        ASSERT(num_elements_ != 0, "Read an invalid element in:" + std::to_string(pthread_self()));
        num_elements_--;
    }

    auto Size() const noexcept { return num_elements_.load(); }

    // Deleted default, copy & move constructors and assignment-operators.
    LockFreeQueue() = delete;

    LockFreeQueue(const LockFreeQueue &) = delete;

    LockFreeQueue(const LockFreeQueue &&) = delete;

    auto operator=(const LockFreeQueue &) -> LockFreeQueue & = delete;

    auto operator=(const LockFreeQueue &&) -> LockFreeQueue & = delete;

   private:
    std::vector<T> store_;

    std::atomic<size_t> next_write_index_ = {0};
    std::atomic<size_t> next_read_index_ = {0};

    std::atomic<size_t> num_elements_ = {0};
};

}  // namespace common
