/*
 * order_manager.hpp
 * The order manager is responsible for managing order state either by sending out orders to the exchange or handling
 * responses. Currently, only allows a single order on each side (buy, sell) at a time.
 */

#pragma once

#include "logging/logger.hpp"
#include "om_order.hpp"
#include "order_gateway/client_response.hpp"
#include "risk_manager.hpp"

namespace trading {

class TradingEngine;

class OrderManager {
   public:
    OrderManager(common::Logger *logger, TradingEngine *trading_engine, trading::RiskManager &risk_manager)
        : trading_engine_(trading_engine), risk_manager_(risk_manager), logger_(logger) {}

    auto OnOrderUpdate(const exchange::MEClientResponse *client_response) noexcept -> void {
        logger_->Log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, common::GetCurrentTimeStr(&time_str_),
                     client_response->ToString().c_str());
        auto order = &(ticker_side_order_.at(client_response->ticker_id_).at(SideToIndex(client_response->side_)));
        logger_->Log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, common::GetCurrentTimeStr(&time_str_),
                     order->ToString().c_str());

        switch (client_response->type_) {
            case exchange::ClientResponseType::ACCEPTED: {
                order->order_state_ = trading::OMOrderState::LIVE;
            } break;
            case exchange::ClientResponseType::CANCELED: {
                order->order_state_ = OMOrderState::DEAD;
            } break;
            case exchange::ClientResponseType::FILLED: {
                order->qty_ = client_response->leaves_qty_;
                if (order->qty_ == 0) {
                    order->order_state_ = OMOrderState::DEAD;
                }
            } break;
            case exchange::ClientResponseType::CANCEL_REJECTED:
            case exchange::ClientResponseType::INVALID: {
            } break;
        }
    }

    auto NewOrder(trading::OMOrder *order, common::TickerId ticker_id, common::Price price, common::Side side,
                  common::Qty qty) noexcept -> void;

    auto CancelOrder(trading::OMOrder *order) noexcept -> void;

    auto MoveOrder(trading::OMOrder *order, common::TickerId ticker_id, common::Price price, common::Side side,
                   common::Qty qty) noexcept {
        switch (order->order_state_) {
            case OMOrderState::LIVE: {
                if (order->price_ != price) {
                    CancelOrder(order);
                }
            } break;
            case OMOrderState::INVALID:
            case OMOrderState::DEAD: {
                if (price != common::PRICE_INVALID) [[likely]] {
                    const auto risk_result = risk_manager_.CheckPreTradeRisk(ticker_id, side, qty);
                    if (risk_result == RiskCheckResult::ALLOWED) [[likely]] {
                        NewOrder(order, ticker_id, price, side, qty);
                    } else {
                        logger_->Log("%:% %() % Ticker:% Side:% Qty:% RiskCheckResult:%\n", __FILE__, __LINE__,
                                     __FUNCTION__, common::GetCurrentTimeStr(&time_str_),
                                     common::TickerIdToString(ticker_id), common::SideToString(side),
                                     common::QtyToString(qty), RiskCheckResultToString(risk_result));
                    }
                }
            } break;
            case OMOrderState::PENDING_NEW:
            case OMOrderState::PENDING_CANCEL:
                break;
        }
    }

    auto MoveOrders(common::TickerId ticker_id, common::Price bid_price, common::Price ask_price,
                    common::Qty clip) noexcept {
        auto bid_order = &(ticker_side_order_.at(ticker_id).at(common::SideToIndex(common::Side::BUY)));
        MoveOrder(bid_order, ticker_id, bid_price, common::Side::BUY, clip);

        auto ask_order = &(ticker_side_order_.at(ticker_id).at(common::SideToIndex(common::Side::SELL)));
        MoveOrder(ask_order, ticker_id, ask_price, common::Side::SELL, clip);
    }

    auto GetOmOrderSideHashMap(common::TickerId ticker_id) const { return &(ticker_side_order_.at(ticker_id)); }

    // Deleted default, copy & move constructors and assignment-operators.
    OrderManager() = delete;

    OrderManager(const OrderManager &) = delete;

    OrderManager(const OrderManager &&) = delete;

    auto operator=(const OrderManager &) -> OrderManager & = delete;

    auto operator=(const OrderManager &&) -> OrderManager & = delete;

   private:
    TradingEngine *trading_engine_ = nullptr;
    const trading::RiskManager &risk_manager_;

    std::string time_str_;
    common::Logger *logger_ = nullptr;

    trading::OMOrderTickerSideMap ticker_side_order_;
    common::OrderId next_order_id_ = 1;
};

}  // namespace trading
