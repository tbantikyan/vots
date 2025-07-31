#pragma once

#include "common/integrity.hpp"
#include "market_data/market_update.hpp"
#include "order_book.hpp"
#include "order_server/client_request.hpp"
#include "order_server/client_response.hpp"
#include "runtime/lock_free_queue.hpp"
#include "runtime/threads.hpp"

namespace exchange {

class MatchingEngine final {
   public:
    MatchingEngine(ClientRequestLFQueue *client_requests, ClientResponseLFQueue *client_responses,
                   MEMarketUpdateLFQueue *market_updates);

    ~MatchingEngine();

    void Start();

    void Stop();

    auto ProcessClientRequest(const MEClientRequest *client_request) noexcept {
        auto order_book = ticker_order_book_[client_request->ticker_id_];
        switch (client_request->type_) {
            case ClientRequestType::NEW: {
                order_book->Add(client_request->client_id_, client_request->order_id_, client_request->ticker_id_,
                                client_request->side_, client_request->price_, client_request->qty_);
            } break;

            case ClientRequestType::CANCEL: {
                order_book->Cancel(client_request->client_id_, client_request->order_id_, client_request->ticker_id_);
            } break;

            default: {
                FATAL("Received invalid client-request-type:" +
                      ClientRequestTypeToString(client_request->type_));
            } break;
        }
    }

    auto SendClientResponse(const MEClientResponse *client_response) noexcept {
        logger_.Log("%:% %() % Sending %\n", __FILE__, __LINE__, __FUNCTION__, common::GetCurrentTimeStr(&time_str_),
                    client_response->ToString());
        auto next_write = outgoing_ogw_responses_->GetNextToWriteTo();
        *next_write = std::move(*client_response);  // NOLINT
        outgoing_ogw_responses_->UpdateWriteIndex();
    }

    auto SendMarketUpdate(const MEMarketUpdate *market_update) noexcept {
        logger_.Log("%:% %() % Sending %\n", __FILE__, __LINE__, __FUNCTION__, common::GetCurrentTimeStr(&time_str_),
                    market_update->ToString());
        auto next_write = outgoing_md_updates_->GetNextToWriteTo();
        *next_write = *market_update;
        outgoing_md_updates_->UpdateWriteIndex();
    }

    auto Run() noexcept {
        logger_.Log("%:% %() %\n", __FILE__, __LINE__, __FUNCTION__, common::GetCurrentTimeStr(&time_str_));
        while (run_) {
            const auto me_client_request = incoming_requests_->GetNextToRead();
            if (me_client_request != nullptr) [[likely]] {
                logger_.Log("%:% %() % Processing %\n", __FILE__, __LINE__, __FUNCTION__,
                            common::GetCurrentTimeStr(&time_str_), me_client_request->ToString());
                ProcessClientRequest(me_client_request);
                incoming_requests_->UpdateReadIndex();
            }
        }
    }

    // Deleted default, copy & move constructors and assignment-operators.
    MatchingEngine() = delete;

    MatchingEngine(const MatchingEngine &) = delete;

    MatchingEngine(const MatchingEngine &&) = delete;

    auto operator=(const MatchingEngine &) -> MatchingEngine & = delete;

    auto operator=(const MatchingEngine &&) -> MatchingEngine & = delete;

   private:
    OrderBookMap ticker_order_book_;

    ClientRequestLFQueue *incoming_requests_ = nullptr;
    ClientResponseLFQueue *outgoing_ogw_responses_ = nullptr;
    MEMarketUpdateLFQueue *outgoing_md_updates_ = nullptr;

    volatile bool run_ = false;

    std::string time_str_;
    common::Logger logger_;
};
}  // namespace exchange
