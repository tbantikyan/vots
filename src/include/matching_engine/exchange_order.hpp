/*
 * exchange_order.hpp
 * Defines the types used in a matching engine order book. Specifically, an Order represents an order submitted by a
 * market participant, while an OrdersAtPrice stores all orders of a side at a given price. Both objects are doubly
 * linked-list nodes.
 */

#pragma once

#include <array>
#include <sstream>

#include "common/types.hpp"

namespace exchange {

struct ExchangeOrder {
    common::TickerId ticker_id_ = common::TICKER_ID_INVALID;
    common::ClientId client_id_ = common::CLIENT_ID_INVALID;
    common::OrderId client_order_id_ = common::ORDER_ID_INVALID;
    common::OrderId market_order_id_ = common::ORDER_ID_INVALID;
    common::Side side_ = common::Side::INVALID;
    common::Price price_ = common::PRICE_INVALID;
    common::Qty qty_ = common::QTY_INVALID;
    common::Priority priority_ = common::PRIORITY_INVALID;

    ExchangeOrder *prev_order_ = nullptr;
    ExchangeOrder *next_order_ = nullptr;

    // only needed for use with MemPool.
    ExchangeOrder() = default;

    ExchangeOrder(common::TickerId ticker_id, common::ClientId client_id, common::OrderId client_order_id,
                  common::OrderId market_order_id, common::Side side, common::Price price, common::Qty qty,
                  common::Priority priority, ExchangeOrder *prev_order, ExchangeOrder *next_order) noexcept
        : ticker_id_(ticker_id),
          client_id_(client_id),
          client_order_id_(client_order_id),
          market_order_id_(market_order_id),
          side_(side),
          price_(price),
          qty_(qty),
          priority_(priority),
          prev_order_(prev_order),
          next_order_(next_order) {}

    auto ToString() const -> std::string {
        std::stringstream ss;
        ss << "MEOrder"
           << "["
           << "ticker:" << common::TickerIdToString(ticker_id_) << " "
           << "cid:" << common::ClientIdToString(client_id_) << " "
           << "oid:" << common::OrderIdToString(client_order_id_) << " "
           << "moid:" << common::OrderIdToString(market_order_id_) << " "
           << "side:" << common::SideToString(side_) << " "
           << "price:" << common::PriceToString(price_) << " "
           << "qty:" << common::QtyToString(qty_) << " "
           << "prio:" << common::PriorityToString(priority_) << " "
           << "prev:"
           << common::OrderIdToString((prev_order_ != nullptr) ? prev_order_->market_order_id_
                                                               : common::ORDER_ID_INVALID)
           << " "
           << "next:"
           << common::OrderIdToString((next_order_ != nullptr) ? next_order_->market_order_id_
                                                               : common::ORDER_ID_INVALID)
           << "]";

        return ss.str();
    }
};

// Mapping from OrderId to an Order.
using OrderMap = std::array<ExchangeOrder *, common::ME_MAX_ORDER_IDS>;
// Mapping from ClientId to all the participants Orders mapped by OrderId.
using ClientOrderMap = std::array<OrderMap, common::ME_MAX_NUM_CLIENTS>;

struct OrdersAtPrice {
    common::Side side_ = common::Side::INVALID;
    common::Price price_ = common::PRICE_INVALID;

    ExchangeOrder *first_order_ = nullptr;

    OrdersAtPrice *prev_entry_ = nullptr;
    OrdersAtPrice *next_entry_ = nullptr;

    OrdersAtPrice() = default;

    OrdersAtPrice(common::Side side, common::Price price, ExchangeOrder *first_me_order, OrdersAtPrice *prev_entry,
                  OrdersAtPrice *next_entry)
        : side_(side), price_(price), first_order_(first_me_order), prev_entry_(prev_entry), next_entry_(next_entry) {}

    auto ToString() const {
        std::stringstream ss;
        ss << "MEOrdersAtPrice["
           << "side:" << common::SideToString(side_) << " "
           << "price:" << common::PriceToString(price_) << " "
           << "first_me_order:" << ((first_order_ != nullptr) ? first_order_->ToString() : "null") << " "
           << "prev:" << common::PriceToString((prev_entry_ != nullptr) ? prev_entry_->price_ : common::PRICE_INVALID)
           << " "
           << "next:" << common::PriceToString((next_entry_ != nullptr) ? next_entry_->price_ : common::PRICE_INVALID)
           << "]";

        return ss.str();
    }
};

// Mapping from Price to OrdersAtPrice.
using OrdersAtPriceMap = std::array<OrdersAtPrice *, common::ME_MAX_PRICE_LEVELS>;

}  // namespace exchange
