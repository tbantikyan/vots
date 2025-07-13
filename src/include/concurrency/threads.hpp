#pragma once

#include <sys/syscall.h>
#include <unistd.h>

#include <atomic>
#include <iostream>
#include <thread>

namespace common {

/*
 * threads.hpp
 * Utility functions for creating threads bound to a specific CPU core.
 */

// SetThreadCore attempts to set thread affinity (i.e. the core on which the thread will execute). Expects to be called
// from within the thread whose affinity is being set.
inline auto SetThreadCore(int core_id) noexcept {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);  // zero out all flags
    CPU_SET(core_id, &cpuset);
    return (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0);
}

// CreateAndStartThread creates a thread running the provided function and with the core affinity {core_id}. Blocks
// until the thread either starts successfully or fails. Upon failure, nullptr is returned.
template <typename FuncType, typename... ArgsType>
inline auto CreateAndStartThread(int core_id, const std::string &name, FuncType &&func, ArgsType &&...args) noexcept
    -> std::thread * {
    std::atomic<bool> running(false);
    std::atomic<bool> failed(false);
    auto thread_body = [&] {
        if (core_id >= 0 && !SetThreadCore(core_id)) {
            std::cerr << "Failed to set core affinity for " << name << " " << pthread_self() << " to " << core_id
                      << '\n';
            failed = true;
            return;
        }
        std::cout << "Set core affinity for " << name << " " << pthread_self() << " to " << core_id << '\n';
        running = true;
        std::forward<FuncType>(func)((std::forward<ArgsType>(args))...);
    };

    auto t = new std::thread(thread_body);  // NOLINT
    while (!running && !failed) {
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(1s);
    }
    if (failed) {
        t->join();
        delete t;
        t = nullptr;
    }
    return t;
}

}  // namespace common
