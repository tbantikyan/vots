#include "trading_engine/trading_engine.hpp"

#include "trading_engine/liquidity_taker.hpp"
#include "trading_engine/market_maker.hpp"

namespace trading {

TradingEngine::TradingEngine(common::ClientId client_id, common::AlgoType algo_type,
                             const common::TradeEngineCfgMap &ticker_cfg,
                             exchange::ClientRequestLFQueue *client_requests,
                             exchange::ClientResponseLFQueue *client_responses,
                             exchange::MEMarketUpdateLFQueue *market_updates)
    : CLIENT_ID(client_id),
      outgoing_ogw_requests_(client_requests),
      incoming_ogw_responses_(client_responses),
      incoming_md_updates_(market_updates),
      logger_("trading_engine_" + std::to_string(client_id) + ".log"),
      feature_engine_(&logger_),
      position_keeper_(&logger_),
      order_manager_(&logger_, this, risk_manager_),
      risk_manager_(&position_keeper_, ticker_cfg) {
    for (size_t i = 0; i < ticker_order_book_.size(); ++i) {
        ticker_order_book_[i] = new TradingOrderBook(i, &logger_);
        ticker_order_book_[i]->SetTradingEngine(this);
    }

    algo_on_order_book_update_ = [this](auto ticker_id, auto price, auto side, auto book) {
        DefaultAlgoOnOrderBookUpdate(ticker_id, price, side, book);
    };
    algo_on_trade_update_ = [this](auto market_update, auto book) { DefaultAlgoOnTradeUpdate(market_update, book); };
    algo_on_order_update_ = [this](auto client_response) { DefaultAlgoOnOrderUpdate(client_response); };

    if (algo_type == common::AlgoType::MAKER) {
        mm_algo_ = new MarketMaker(&logger_, this, &feature_engine_, &order_manager_, ticker_cfg);
    } else if (algo_type == common::AlgoType::TAKER) {
        taker_algo_ = new LiquidityTaker(&logger_, this, &feature_engine_, &order_manager_, ticker_cfg);
    }

    for (common::TickerId i = 0; i < ticker_cfg.size(); ++i) {
        logger_.Log("%:% %() % Initialized % Ticker:% %.\n", __FILE__, __LINE__, __FUNCTION__,
                    common::GetCurrentTimeStr(&time_str_), common::AlgoTypeToString(algo_type), i,
                    ticker_cfg.at(i).ToString());
    }
}

TradingEngine::~TradingEngine() {
    run_ = false;

    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(1s);

    delete mm_algo_;
    mm_algo_ = nullptr;
    delete taker_algo_;
    taker_algo_ = nullptr;

    for (auto &order_book : ticker_order_book_) {
        delete order_book;
        order_book = nullptr;
    }

    outgoing_ogw_requests_ = nullptr;
    incoming_ogw_responses_ = nullptr;
    incoming_md_updates_ = nullptr;
}

void TradingEngine::SendClientRequest(const exchange::MEClientRequest *client_request) noexcept {
    logger_.Log("%:% %() % Sending %\n", __FILE__, __LINE__, __FUNCTION__, common::GetCurrentTimeStr(&time_str_),
                client_request->ToString().c_str());
    auto next_write = outgoing_ogw_requests_->GetNextToWriteTo();
    *next_write = *client_request;
    outgoing_ogw_requests_->UpdateWriteIndex();
}

void TradingEngine::Run() noexcept {
    logger_.Log("%:% %() %\n", __FILE__, __LINE__, __FUNCTION__, common::GetCurrentTimeStr(&time_str_));
    while (run_) {
        for (auto client_response = incoming_ogw_responses_->GetNextToRead(); client_response != nullptr;
             client_response = incoming_ogw_responses_->GetNextToRead()) {
            logger_.Log("%:% %() % Processing %\n", __FILE__, __LINE__, __FUNCTION__,
                        common::GetCurrentTimeStr(&time_str_), client_response->ToString().c_str());
            OnOrderUpdate(client_response);
            incoming_ogw_responses_->UpdateReadIndex();
            last_event_time_ = common::GetCurrentNanos();
        }

        for (auto market_update = incoming_md_updates_->GetNextToRead(); market_update != nullptr;
             market_update = incoming_md_updates_->GetNextToRead()) {
            logger_.Log("%:% %() % Processing %\n", __FILE__, __LINE__, __FUNCTION__,
                        common::GetCurrentTimeStr(&time_str_), market_update->ToString().c_str());
            ASSERT(market_update->ticker_id_ < ticker_order_book_.size(),
                   "Unknown ticker-id on update:" + market_update->ToString());
            ticker_order_book_[market_update->ticker_id_]->OnMarketUpdate(market_update);
            incoming_md_updates_->UpdateReadIndex();
            last_event_time_ = common::GetCurrentNanos();
        }
    }
}

void TradingEngine::OnOrderBookUpdate(common::TickerId ticker_id, common::Price price, common::Side side,
                                      TradingOrderBook *book) noexcept {
    logger_.Log("%:% %() % ticker:% price:% side:%\n", __FILE__, __LINE__, __FUNCTION__,
                common::GetCurrentTimeStr(&time_str_), ticker_id, common::PriceToString(price).c_str(),
                common::SideToString(side).c_str());

    position_keeper_.UpdateBbo(ticker_id, book->GetBbo());

    feature_engine_.OnOrderBookUpdate(ticker_id, price, side, book);

    algo_on_order_book_update_(ticker_id, price, side, book);
}

void TradingEngine::OnTradeUpdate(const exchange::MEMarketUpdate *market_update, TradingOrderBook *book) noexcept {
    logger_.Log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, common::GetCurrentTimeStr(&time_str_),
                market_update->ToString().c_str());

    feature_engine_.OnTradeUpdate(market_update, book);

    algo_on_trade_update_(market_update, book);
}

void TradingEngine::OnOrderUpdate(const exchange::MEClientResponse *client_response) noexcept {
    logger_.Log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, common::GetCurrentTimeStr(&time_str_),
                client_response->ToString().c_str());

    if (client_response->type_ == exchange::ClientResponseType::FILLED) [[unlikely]] {
        position_keeper_.AddFill(client_response);
    }

    algo_on_order_update_(client_response);
}

}  // namespace trading
