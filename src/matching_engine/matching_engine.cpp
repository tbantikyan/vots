#include "matching_engine/matching_engine.hpp"

namespace exchange {

MatchingEngine::MatchingEngine(ClientRequestLFQueue *client_requests, ClientResponseLFQueue *client_responses,
                               MEMarketUpdateLFQueue *market_updates)
    : incoming_requests_(client_requests),
      outgoing_ogw_responses_(client_responses),
      outgoing_md_updates_(market_updates),
      logger_("exchange_matching_engine.log") {
    for (size_t i = 0; i < ticker_order_book_.size(); ++i) {
        ticker_order_book_[i] = new ExchangeOrderBook(i, &logger_, this);
    }
}

MatchingEngine::~MatchingEngine() {
    run_ = false;

    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(1s);

    incoming_requests_ = nullptr;
    outgoing_ogw_responses_ = nullptr;
    outgoing_md_updates_ = nullptr;

    for (auto &order_book : ticker_order_book_) {
        delete order_book;
        order_book = nullptr;
    }
}

void MatchingEngine::Start() {
    run_ = true;
    ASSERT(common::CreateAndStartThread(-1, "exchange/MatchingEngine", [this]() { Run(); }) != nullptr,
           "Failed to start MatchingEngine thread.");
}

void MatchingEngine::Stop() { run_ = false; }

}  // namespace exchange
