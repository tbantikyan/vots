/*
 * liquidity_taker.hpp
 * Defines the Liquidity Taker trading strategy that tries to profit by crossing the spread. The sole feature for this
 * simple implementation is trying to follow in the same direction as large trades.
 */

#pragma once

#include "common/perf_utils.hpp"
#include "feature_engine.hpp"
#include "logging/logger.hpp"
#include "order_manager.hpp"
#include "trading_engine.hpp"

namespace trading {

class LiquidityTaker {
   public:
    LiquidityTaker(common::Logger *logger, TradingEngine *trading_engine, const FeatureEngine *feature_engine,
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

    void OnOrderBookUpdate(common::TickerId ticker_id, common::Price price, common::Side side,
                           TradingOrderBook * /*unused*/) noexcept {
        logger_->Log("%:% %() % ticker:% price:% side:%\n", __FILE__, __LINE__, __FUNCTION__,
                     common::GetCurrentTimeStr(&time_str_), ticker_id, common::PriceToString(price).c_str(),
                     common::SideToString(side).c_str());
    }

    void OnTradeUpdate(const exchange::MEMarketUpdate *market_update, TradingOrderBook *book) noexcept {
        logger_->Log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, common::GetCurrentTimeStr(&time_str_),
                     market_update->ToString().c_str());

        const auto bbo = book->GetBbo();
        const auto agg_qty_ratio = feature_engine_->GetAggTradeQtyRatio();

        if (bbo->bid_price_ != common::PRICE_INVALID && bbo->ask_price_ != common::PRICE_INVALID &&
            agg_qty_ratio != FEATURE_INVALID) [[likely]] {
            logger_->Log("%:% %() % % agg-qty-ratio:%\n", __FILE__, __LINE__, __FUNCTION__,
                         common::GetCurrentTimeStr(&time_str_), bbo->ToString().c_str(), agg_qty_ratio);

            const auto clip = TICKER_CFG.at(market_update->ticker_id_).clip_;
            const auto threshold = TICKER_CFG.at(market_update->ticker_id_).threshold_;

            if (agg_qty_ratio >= threshold) {
                START_MEASURE(trading_order_manager_move_orders);
                if (market_update->side_ == common::Side::BUY) {
                    order_manager_->MoveOrders(market_update->ticker_id_, bbo->ask_price_, common::PRICE_INVALID, clip);
                } else {
                    order_manager_->MoveOrders(market_update->ticker_id_, common::PRICE_INVALID, bbo->bid_price_, clip);
                }
                END_MEASURE(trading_order_manager_move_orders, (*logger_));
            }
        }
    }

    void OnOrderUpdate(const exchange::MEClientResponse *client_response) noexcept {
        logger_->Log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, common::GetCurrentTimeStr(&time_str_),
                     client_response->ToString().c_str());
        START_MEASURE(trading_order_manager_on_order_update);
        order_manager_->OnOrderUpdate(client_response);
        END_MEASURE(trading_order_manager_on_order_update, (*logger_));
    }

    // Deleted default, copy & move constructors and assignment-operators.
    LiquidityTaker() = delete;

    LiquidityTaker(const LiquidityTaker &) = delete;

    LiquidityTaker(const LiquidityTaker &&) = delete;

    auto operator=(const LiquidityTaker &) -> LiquidityTaker & = delete;

    auto operator=(const LiquidityTaker &&) -> LiquidityTaker & = delete;

   private:
    const FeatureEngine *feature_engine_ = nullptr;
    OrderManager *order_manager_ = nullptr;

    std::string time_str_;
    common::Logger *logger_ = nullptr;

    const common::TradeEngineCfgMap TICKER_CFG;
};

}  // namespace trading
