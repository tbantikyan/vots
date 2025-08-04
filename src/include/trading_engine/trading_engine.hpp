#pragma once

#include <functional>

#include "common/integrity.hpp"
#include "feature_engine.hpp"
#include "logging/logger.hpp"
#include "common/time_utils.hpp"
#include "market_data/market_update.hpp"
#include "order_gateway/client_request.hpp"
#include "order_gateway/client_response.hpp"
#include "order_manager.hpp"
#include "position_keeper.hpp"
#include "risk_manager.hpp"
#include "runtime/lock_free_queue.hpp"
#include "runtime/threads.hpp"
#include "trading_order_book.hpp"

namespace trading {

class MarketMaker;
class LiquidityTaker;

class TradingEngine {
   public:
    TradingEngine(common::ClientId client_id, common::AlgoType algo_type, const common::TradeEngineCfgMap &ticker_cfg,
                  exchange::ClientRequestLFQueue *client_requests, exchange::ClientResponseLFQueue *client_responses,
                  exchange::MEMarketUpdateLFQueue *market_updates);

    ~TradingEngine();

    void Start() {
        run_ = true;
        ASSERT(common::CreateAndStartThread(-1, "Trading/TradeEngine", [this] { Run(); }) != nullptr,
               "Failed to start TradeEngine thread.");
    }

    void Stop() {
        while ((incoming_ogw_responses_->Size() != 0) || (incoming_md_updates_->Size() != 0)) {
            logger_.Log("%:% %() % Sleeping till all updates are consumed ogw-size:% md-size:%\n", __FILE__, __LINE__,
                        __FUNCTION__, common::GetCurrentTimeStr(&time_str_), incoming_ogw_responses_->Size(),
                        incoming_md_updates_->Size());

            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(10ms);
        }

        logger_.Log("%:% %() % POSITIONS\n%\n", __FILE__, __LINE__, __FUNCTION__, common::GetCurrentTimeStr(&time_str_),
                    position_keeper_.ToString());

        run_ = false;
    }

    void Run() noexcept;

    void SendClientRequest(const exchange::MEClientRequest *client_request) noexcept;

    void OnOrderBookUpdate(common::TickerId ticker_id, common::Price price, common::Side side,
                           TradingOrderBook *book) noexcept;
    void OnTradeUpdate(const exchange::MEMarketUpdate *market_update, TradingOrderBook *book) noexcept;

    void OnOrderUpdate(const exchange::MEClientResponse *client_response) noexcept;

    std::function<void(common::TickerId ticker_id, common::Price price, common::Side side, TradingOrderBook *book)>
        algo_on_order_book_update_;
    std::function<void(const exchange::MEMarketUpdate *market_update, TradingOrderBook *book)> algo_on_trade_update_;
    std::function<void(const exchange::MEClientResponse *client_response)> algo_on_order_update_;

    auto InitLastEventTime() { last_event_time_ = common::GetCurrentNanos(); }

    auto SilentSeconds() { return (common::GetCurrentNanos() - last_event_time_) / common::NANOS_TO_SECS; }

    auto ClientId() const { return CLIENT_ID; }

    // Deleted default, copy & move constructors and assignment-operators.
    TradingEngine() = delete;

    TradingEngine(const TradingEngine &) = delete;

    TradingEngine(const TradingEngine &&) = delete;

    auto operator=(const TradingEngine &) -> TradingEngine & = delete;

    auto operator=(const TradingEngine &&) -> TradingEngine & = delete;

   private:
    const common::ClientId CLIENT_ID;

    TradingOrderBookMap ticker_order_book_;

    exchange::ClientRequestLFQueue *outgoing_ogw_requests_ = nullptr;
    exchange::ClientResponseLFQueue *incoming_ogw_responses_ = nullptr;
    exchange::MEMarketUpdateLFQueue *incoming_md_updates_ = nullptr;

    common::Nanos last_event_time_ = 0;
    volatile bool run_ = false;

    std::string time_str_;
    common::Logger logger_;

    FeatureEngine feature_engine_;
    PositionKeeper position_keeper_;
    OrderManager order_manager_;
    RiskManager risk_manager_;

    MarketMaker *mm_algo_ = nullptr;
    LiquidityTaker *taker_algo_ = nullptr;

    void DefaultAlgoOnOrderBookUpdate(common::TickerId ticker_id, common::Price price, common::Side side,
                                      TradingOrderBook * /*unused*/) noexcept {
        logger_.Log("%:% %() % ticker:% price:% side:%\n", __FILE__, __LINE__, __FUNCTION__,
                    common::GetCurrentTimeStr(&time_str_), ticker_id, common::PriceToString(price).c_str(),
                    common::SideToString(side).c_str());
    }

    void DefaultAlgoOnTradeUpdate(const exchange::MEMarketUpdate *market_update,
                                  TradingOrderBook * /*unused*/) noexcept {
        logger_.Log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, common::GetCurrentTimeStr(&time_str_),
                    market_update->ToString().c_str());
    }

    void DefaultAlgoOnOrderUpdate(const exchange::MEClientResponse *client_response) noexcept {
        logger_.Log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, common::GetCurrentTimeStr(&time_str_),
                    client_response->ToString().c_str());
    }
};

}  // namespace trading
