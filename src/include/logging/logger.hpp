/*
 * logger.hpp
 * Provides a flexible logging type that moves I/O operations to a single background thread.
 */

#pragma once

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

#include "common/integrity.hpp"
#include "common/time_utils.hpp"
#include "log_type.hpp"
#include "runtime/lock_free_queue.hpp"
#include "runtime/threads.hpp"

namespace common {

constexpr size_t LOG_QUEUE_SIZE = 8 * 1024 * 1024;

/*
 * Logger is a fixed-sized, asynchronous logging framework that supports a few primitive types and some basic message
 * formatting. Uses a lock-free queue for efficient communication (i.e. context switch free) with the background thread.
 */
class Logger final {
   public:
    struct Element {
        // Although variants are a modern abstraction of this logic, they offer less runtime performance than manually
        // managing a union.
        LogType type_ = LogType::CHAR;
        union {
            char c_;
            int i_;
            long l_;
            long long ll_;
            unsigned u_;
            unsigned long ul_;
            unsigned long long ull_;
            float f_;
            double d_;
            char s_[256];
        } u_;
    };

    auto FlushQueue() noexcept {
        while (running_) {
            for (auto next = queue_.GetNextToRead(); queue_.Size() != 0 && (next != nullptr);
                 next = queue_.GetNextToRead()) {
                switch (next->type_) {
                    case LogType::CHAR:
                        file_ << next->u_.c_;
                        break;
                    case LogType::INTEGER:
                        file_ << next->u_.i_;
                        break;
                    case LogType::LONG_INTEGER:
                        file_ << next->u_.l_;
                        break;
                    case LogType::LONG_LONG_INTEGER:
                        file_ << next->u_.ll_;
                        break;
                    case LogType::UNSIGNED_INTEGER:
                        file_ << next->u_.u_;
                        break;
                    case LogType::UNSIGNED_LONG_INTEGER:
                        file_ << next->u_.ul_;
                        break;
                    case LogType::UNSIGNED_LONG_LONG_INTEGER:
                        file_ << next->u_.ull_;
                        break;
                    case LogType::FLOAT:
                        file_ << next->u_.f_;
                        break;
                    case LogType::DOUBLE:
                        file_ << next->u_.d_;
                        break;
                    case LogType::STRING:
                        file_ << next->u_.s_;
                        break;
                }
                queue_.UpdateReadIndex();
            }
            file_.flush();

            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(10ms);
        }
    }

    explicit Logger(const std::string &file_name) : FILE_NAME(file_name), queue_(LOG_QUEUE_SIZE) {
        file_.open(file_name);
        ASSERT(file_.is_open(), "Could not open log file:" + file_name);
        logger_thread_ = CreateAndStartThread(-1, "common/Logger " + FILE_NAME, [this]() { FlushQueue(); });
        ASSERT(logger_thread_ != nullptr, "Failed to start Logger thread.");
    }

    ~Logger() {
        std::string time_str;
        std::cerr << common::GetCurrentTimeStr(&time_str) << " Flushing and closing Logger for " << FILE_NAME << '\n';

        while (queue_.Size() != 0) {
            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(1s);
        }
        running_ = false;
        logger_thread_->join();

        file_.close();
        std::cerr << common::GetCurrentTimeStr(&time_str) << " Logger for " << FILE_NAME << " exiting." << '\n';
    }

    auto PushValue(const Element &log_element) noexcept {
        *(queue_.GetNextToWriteTo()) = log_element;
        queue_.UpdateWriteIndex();
    }

    auto PushValue(const char value) noexcept { PushValue(Element{.type_ = LogType::CHAR, .u_ = {.c_ = value}}); }

    auto PushValue(const int value) noexcept { PushValue(Element{.type_ = LogType::INTEGER, .u_ = {.i_ = value}}); }

    auto PushValue(const long value) noexcept {
        PushValue(Element{.type_ = LogType::LONG_INTEGER, .u_ = {.l_ = value}});
    }

    auto PushValue(const long long value) noexcept {
        PushValue(Element{.type_ = LogType::LONG_LONG_INTEGER, .u_ = {.ll_ = value}});
    }

    auto PushValue(const unsigned value) noexcept {
        PushValue(Element{.type_ = LogType::UNSIGNED_INTEGER, .u_ = {.u_ = value}});
    }

    auto PushValue(const unsigned long value) noexcept {
        PushValue(Element{.type_ = LogType::UNSIGNED_LONG_INTEGER, .u_ = {.ul_ = value}});
    }

    auto PushValue(const unsigned long long value) noexcept {
        PushValue(Element{.type_ = LogType::UNSIGNED_LONG_LONG_INTEGER, .u_ = {.ull_ = value}});
    }

    auto PushValue(const float value) noexcept { PushValue(Element{.type_ = LogType::FLOAT, .u_ = {.f_ = value}}); }

    auto PushValue(const double value) noexcept { PushValue(Element{.type_ = LogType::DOUBLE, .u_ = {.d_ = value}}); }

    auto PushValue(const char *value) noexcept {
        Element l{.type_=LogType::STRING, .u_={.s_ = {}}};
        strncpy(l.u_.s_, value, sizeof(l.u_.s_) - 1);
        PushValue(l);
    }

    auto PushValue(const std::string &value) noexcept { PushValue(value.c_str()); }

    template <typename T, typename... A>
    auto Log(const char *s, const T &value, A... args) noexcept {
        while (*s) {
            if (*s == '%') {
                if (*(s + 1) == '%') [[unlikely]] {  // to allow %% -> % escape character.
                    ++s;
                } else {
                    PushValue(value);     // substitute % with the value specified in the arguments.
                    Log(s + 1, args...);  // pop an argument and call self recursively.
                    return;
                }
            }
            PushValue(*s++);
        }
        FATAL("extra arguments provided to log()");
    }

    auto Log(const char *s) noexcept {
        while (*s != 0U) {
            if (*s == '%') {
                if (*(s + 1) == '%') [[unlikely]] {  // to allow %% -> % escape character.
                    ++s;
                } else {
                    FATAL("missing arguments to log()");
                }
            }
            PushValue(*s++);
        }
    }

    // Deleted default, copy & move constructors and assignment-operators.
    Logger() = delete;

    Logger(const Logger &) = delete;

    Logger(const Logger &&) = delete;

    auto operator=(const Logger &) -> Logger & = delete;

    auto operator=(const Logger &&) -> Logger & = delete;

   private:
    const std::string FILE_NAME;
    std::ofstream file_;

    LockFreeQueue<Element> queue_;
    std::atomic<bool> running_ = {true};
    std::thread *logger_thread_ = nullptr;
};

}  // namespace common
