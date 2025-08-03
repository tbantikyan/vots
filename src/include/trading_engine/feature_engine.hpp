/*
 * feature_engine.hpp
 * Basic feature engine that calculates Fair Market Price and Aggressive Trade Quantity Ratio.
 */

#pragma once

#include "common/types.hpp"
#include "logging/logger.hpp"
#include "trading_order_book.hpp"

namespace trading {

constexpr auto FEATURE_INVALID = std::numeric_limits<double>::quiet_NaN();

class FeatureEngine {
   public:
    explicit FeatureEngine(common::Logger *logger) : logger_(logger) {}

    auto OnOrderBookUpdate(common::TickerId ticker_id, common::Price price, common::Side side,
                           TradingOrderBook *book) noexcept -> void {
        const auto bbo = book->GetBbo();
        if (bbo->bid_price_ != common::PRICE_INVALID && bbo->ask_price_ != common::PRICE_INVALID) [[likely]] {
            mkt_price_ = (bbo->bid_price_ * bbo->ask_qty_ + bbo->ask_price_ * bbo->bid_qty_) /
                         static_cast<double>(bbo->bid_qty_ + bbo->ask_qty_);
        }

        logger_->Log("%:% %() % ticker:% price:% side:% mkt-price:% agg-trade-ratio:%\n", __FILE__, __LINE__,
                     __FUNCTION__, common::GetCurrentTimeStr(&time_str_), ticker_id,
                     common::PriceToString(price).c_str(), common::SideToString(side).c_str(), mkt_price_,
                     agg_trade_qty_ratio_);
    }

    auto OnTradeUpdate(const exchange::MEMarketUpdate *market_update, TradingOrderBook *book) noexcept -> void {
        const auto bbo = book->GetBbo();
        if (bbo->bid_price_ != common::PRICE_INVALID && bbo->ask_price_ != common::PRICE_INVALID) [[likely]] {
            agg_trade_qty_ratio_ = static_cast<double>(market_update->qty_) /
                                   (market_update->side_ == common::Side::BUY ? bbo->ask_qty_ : bbo->bid_qty_);
        }

        logger_->Log("%:% %() % % mkt-price:% agg-trade-ratio:%\n", __FILE__, __LINE__, __FUNCTION__,
                     common::GetCurrentTimeStr(&time_str_), market_update->ToString().c_str(), mkt_price_,
                     agg_trade_qty_ratio_);
    }

    auto GetMktPrice() const noexcept { return mkt_price_; }

    auto GetAggTradeQtyRatio() const noexcept { return agg_trade_qty_ratio_; }

    // Deleted default, copy & move constructors and assignment-operators.
    FeatureEngine() = delete;

    FeatureEngine(const FeatureEngine &) = delete;

    FeatureEngine(const FeatureEngine &&) = delete;

    auto operator=(const FeatureEngine &) -> FeatureEngine & = delete;

    auto operator=(const FeatureEngine &&) -> FeatureEngine & = delete;

   private:
    std::string time_str_;
    common::Logger *logger_ = nullptr;

    double mkt_price_ = FEATURE_INVALID, agg_trade_qty_ratio_ = FEATURE_INVALID;
};

}  // namespace trading
