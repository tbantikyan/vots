/*
 * memory_pool.hpp
 * Implements a simple memory pool that tracks allocation on-heap.
 */

#pragma once

#include <string>
#include <vector>

#include "common/integrity.hpp"

namespace common {

template <typename T>
class MemoryPool final {
   public:
    explicit MemoryPool(std::size_t num_elems) : store_(num_elems, {T(), true}) {
        // Ensure that object_ is the first member of ObjectBlock such that pointer to object_ points to its owning
        // ObjectBlock, too.
        ASSERT(reinterpret_cast<const ObjectBlock *>(&(store_[0].object_)) == store_.data(),
               "T object should be first member of ObjectBlock.");
    }

    template <typename... Args>
    auto Allocate(Args... args) noexcept -> T * {
        auto obj_block = &(store_[next_free_index_]);
        ASSERT(obj_block->is_free_, "Expected free ObjectBlock at index:" + std::to_string(next_free_index_));
        T *ret = &(obj_block->object_);
        ret = new (ret) T(args...);  // placement new.
        obj_block->is_free_ = false;

        UpdateNextFreeIndex();

        return ret;
    }

    auto Deallocate(const T *elem) noexcept {
        const auto elem_index = (reinterpret_cast<const ObjectBlock *>(elem) - store_.data());
        ASSERT(elem_index >= 0 && static_cast<size_t>(elem_index) < store_.size(),
               "Element being deallocated does not belong to this Memory pool.");
        ASSERT(!store_[elem_index].is_free_, "Expected in-use ObjectBlock at index:" + std::to_string(elem_index));
        store_[elem_index].is_free_ = true;
    }

    // Deleted default, copy & move constructors and assignment-operators.
    MemoryPool() = delete;

    MemoryPool(const MemoryPool &) = delete;

    MemoryPool(const MemoryPool &&) = delete;

    auto operator=(const MemoryPool &) -> MemoryPool & = delete;

    auto operator=(const MemoryPool &&) -> MemoryPool & = delete;

   private:
    struct ObjectBlock {
        T object_;
        bool is_free_ = true;
    };

    // std::array not preferred; It is good to have objects on the stack, but performance starts getting worse as the
    // size of the pool increases.
    std::vector<ObjectBlock> store_;

    size_t next_free_index_ = 0;

    auto UpdateNextFreeIndex() noexcept {
        const auto initial_free_index = next_free_index_;
        while (!store_[next_free_index_].is_free_) {
            ++next_free_index_;

            // hardware branch predictor should typically never predict this --> minimal cost of if.
            if (next_free_index_ == store_.size()) [[unlikely]] {
                next_free_index_ = 0;
            }
            if (initial_free_index == next_free_index_) [[unlikely]] {
                ASSERT(initial_free_index != next_free_index_, "Memory Pool out of space.");
            }
        }
    }
};
}  // namespace common
