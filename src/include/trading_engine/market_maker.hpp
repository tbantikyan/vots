/*
 * market_maker.hpp
 * Defines the Market Maker trading strategy that tries to profit by capturing the spread. This algorithm checks the
 * fair market price to determine whether to send an order at the best price or a subpar price.
 */

#pragma once

#include "feature_engine.hpp"
#include "logging/logger.hpp"
#include "order_manager.hpp"
#include "trading_engine.hpp"

namespace trading {

class MarketMaker {
   public:
    MarketMaker(common::Logger *logger, TradingEngine *trading_engine, const FeatureEngine *feature_engine,
                OrderManager *order_manager, const common::TradeEngineCfgMap &ticker_cfg)
        : feature_engine_(feature_engine), order_manager_(order_manager), logger_(logger), TICKER_CFG(ticker_cfg) {
        trading_engine->algo_on_order_book_update_ = [this](auto ticker_id, auto price, auto side, auto book) {
            OnOrderBookUpdate(ticker_id, price, side, book);
        };
        trading_engine->algo_on_trade_update_ = [this](auto market_update, auto book) {
            OnTradeUpdate(market_update, book);
        };
        trading_engine->algo_on_order_update_ = [this](auto client_response) { OnOrderUpdate(client_response); };
    }

    auto OnOrderBookUpdate(common::TickerId ticker_id, common::Price price, common::Side side,
                           const TradingOrderBook *book) noexcept -> void {
        logger_->Log("%:% %() % ticker:% price:% side:%\n", __FILE__, __LINE__, __FUNCTION__,
                     common::GetCurrentTimeStr(&time_str_), ticker_id, common::PriceToString(price).c_str(),
                     common::SideToString(side).c_str());

        const auto bbo = book->GetBbo();
        const auto fair_price = feature_engine_->GetMktPrice();

        if (bbo->bid_price_ != common::PRICE_INVALID && bbo->ask_price_ != common::PRICE_INVALID &&
            fair_price != FEATURE_INVALID) [[likely]] {
            logger_->Log("%:% %() % % fair-price:%\n", __FILE__, __LINE__, __FUNCTION__,
                         common::GetCurrentTimeStr(&time_str_), bbo->ToString().c_str(), fair_price);

            const auto clip = TICKER_CFG.at(ticker_id).clip_;
            const auto threshold = TICKER_CFG.at(ticker_id).threshold_;

            const auto bid_price = bbo->bid_price_ - (fair_price - bbo->bid_price_ >= threshold ? 0 : 1);
            const auto ask_price = bbo->ask_price_ + (bbo->ask_price_ - fair_price >= threshold ? 0 : 1);

            order_manager_->MoveOrders(ticker_id, bid_price, ask_price, clip);
        }
    }

    auto OnTradeUpdate(const exchange::MEMarketUpdate *market_update, TradingOrderBook * /* unused */) noexcept
        -> void {
        logger_->Log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, common::GetCurrentTimeStr(&time_str_),
                     market_update->ToString().c_str());
    }

    auto OnOrderUpdate(const exchange::MEClientResponse *client_response) noexcept -> void {
        logger_->Log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, common::GetCurrentTimeStr(&time_str_),
                     client_response->ToString().c_str());
        order_manager_->OnOrderUpdate(client_response);
    }

    // Deleted default, copy & move constructors and assignment-operators.
    MarketMaker() = delete;

    MarketMaker(const MarketMaker &) = delete;

    MarketMaker(const MarketMaker &&) = delete;

    auto operator=(const MarketMaker &) -> MarketMaker & = delete;

    auto operator=(const MarketMaker &&) -> MarketMaker & = delete;

   private:
    const FeatureEngine *feature_engine_ = nullptr;
    OrderManager *order_manager_ = nullptr;

    std::string time_str_;
    common::Logger *logger_ = nullptr;

    const common::TradeEngineCfgMap TICKER_CFG;
};

}  // namespace trading
