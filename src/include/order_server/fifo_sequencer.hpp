#pragma once

#include "client_request.hpp"
#include "common/integrity.hpp"
#include "logging/logger.hpp"
#include "runtime/threads.hpp"

namespace exchange {

constexpr size_t ME_MAX_PENDING_REQUESTS = 1024;

class FIFOSequencer {
   private:
    // needs to be defined before sort call down below on pending_client_requests_
    struct RecvTimeClientRequest {
        common::Nanos recv_time_ = 0;
        MEClientRequest request_;

        auto operator<(const RecvTimeClientRequest &rhs) const { return (recv_time_ < rhs.recv_time_); }
    };

   public:
    FIFOSequencer(ClientRequestLFQueue *client_requests, common::Logger *logger)
        : incoming_requests_(client_requests), logger_(logger) {}

    ~FIFOSequencer() = default;

    auto AddClientRequest(common::Nanos rx_time, const MEClientRequest &request) {
        if (pending_size_ >= pending_client_requests_.size()) {
            FATAL("Too many pending requests");
        }
        pending_client_requests_.at(pending_size_++) =
            RecvTimeClientRequest{.recv_time_ = rx_time, .request_ = request};
    }

    auto SequenceAndPublish() {
        if (pending_size_ == 0) [[unlikely]] {
            return;
        }

        logger_->Log("%:% %() % Processing % requests.\n", __FILE__, __LINE__, __FUNCTION__,
                     common::GetCurrentTimeStr(&time_str_), pending_size_);

        std::sort(pending_client_requests_.begin(), pending_client_requests_.begin() + pending_size_);

        for (size_t i = 0; i < pending_size_; ++i) {
            const auto &client_request = pending_client_requests_.at(i);

            logger_->Log("%:% %() % Writing RX:% Req:% to FIFO.\n", __FILE__, __LINE__, __FUNCTION__,
                         common::GetCurrentTimeStr(&time_str_), client_request.recv_time_,
                         client_request.request_.ToString());

            auto next_write = incoming_requests_->GetNextToWriteTo();
            *next_write = client_request.request_;
            incoming_requests_->UpdateWriteIndex();
        }

        pending_size_ = 0;
    }

    // Deleted default, copy & move constructors and assignment-operators.
    FIFOSequencer() = delete;

    FIFOSequencer(const FIFOSequencer &) = delete;

    FIFOSequencer(const FIFOSequencer &&) = delete;

    auto operator=(const FIFOSequencer &) -> FIFOSequencer & = delete;

    auto operator=(const FIFOSequencer &&) -> FIFOSequencer & = delete;

   private:
    ClientRequestLFQueue *incoming_requests_ = nullptr;

    std::string time_str_;
    common::Logger *logger_ = nullptr;

    std::array<RecvTimeClientRequest, ME_MAX_PENDING_REQUESTS> pending_client_requests_;
    size_t pending_size_ = 0;
};

}  // namespace exchange
