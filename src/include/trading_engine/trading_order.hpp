/*
 * trading_order.hpp
 * Defines the types used in a trading engine order book. Specifically, a TradingOrder represents an order submitted by
 * a market participant, while an OrdersAtPrice stores all orders of a side at a given price. Both objects are doubly
 * linked-list nodes. This is a struct is a subset of the ExchangeOrder type.
 *
 * Also defines a BBO data structure for use with various trading engine components that make decisions off the data.
 */

#pragma once

#include <array>
#include <sstream>

#include "common/types.hpp"

namespace trading {

struct TradingOrder {
    common::OrderId order_id_ = common::ORDER_ID_INVALID;
    common::Side side_ = common::Side::INVALID;
    common::Price price_ = common::PRICE_INVALID;
    common::Qty qty_ = common::QTY_INVALID;
    common::Priority priority_ = common::PRIORITY_INVALID;

    TradingOrder *prev_order_ = nullptr;
    TradingOrder *next_order_ = nullptr;

    // only needed for use with MemoryPool.
    TradingOrder() = default;

    TradingOrder(common::OrderId order_id, common::Side side, common::Price price, common::Qty qty,
                 common::Priority priority, TradingOrder *prev_order, TradingOrder *next_order) noexcept
        : order_id_(order_id),
          side_(side),
          price_(price),
          qty_(qty),
          priority_(priority),
          prev_order_(prev_order),
          next_order_(next_order) {}

    auto ToString() const -> std::string {
        std::stringstream ss;
        ss << "TradingOrder"
           << "["
           << "oid:" << common::OrderIdToString(order_id_) << " "
           << "side:" << common::SideToString(side_) << " "
           << "price:" << common::PriceToString(price_) << " "
           << "qty:" << common::QtyToString(qty_) << " "
           << "prio:" << common::PriorityToString(priority_) << " "
           << "prev:"
           << common::OrderIdToString((prev_order_ != nullptr) ? prev_order_->order_id_ : common::ORDER_ID_INVALID)
           << " "
           << "next:"
           << common::OrderIdToString((next_order_ != nullptr) ? next_order_->order_id_ : common::ORDER_ID_INVALID)
           << "]";

        return ss.str();
    }
};

using OrderMap = std::array<TradingOrder *, common::ME_MAX_ORDER_IDS>;

struct TradingOrdersAtPrice {
    common::Side side_ = common::Side::INVALID;
    common::Price price_ = common::PRICE_INVALID;

    TradingOrder *first_mkt_order_ = nullptr;

    TradingOrdersAtPrice *prev_entry_ = nullptr;
    TradingOrdersAtPrice *next_entry_ = nullptr;

    TradingOrdersAtPrice() = default;

    TradingOrdersAtPrice(common::Side side, common::Price price, TradingOrder *first_mkt_order,
                        TradingOrdersAtPrice *prev_entry, TradingOrdersAtPrice *next_entry)
        : side_(side),
          price_(price),
          first_mkt_order_(first_mkt_order),
          prev_entry_(prev_entry),
          next_entry_(next_entry) {}

    auto ToString() const {
        std::stringstream ss;
        ss << "TradingOrdersAtPrice["
           << "side:" << common::SideToString(side_) << " "
           << "price:" << common::PriceToString(price_) << " "
           << "first_mkt_order:" << ((first_mkt_order_ != nullptr) ? first_mkt_order_->ToString() : "null") << " "
           << "prev:" << common::PriceToString((prev_entry_ != nullptr) ? prev_entry_->price_ : common::PRICE_INVALID)
           << " "
           << "next:" << common::PriceToString((next_entry_ != nullptr) ? next_entry_->price_ : common::PRICE_INVALID)
           << "]";

        return ss.str();
    }
};

using OrdersAtPriceMap = std::array<TradingOrdersAtPrice *, common::ME_MAX_PRICE_LEVELS>;

// Best-Bid Offer data structure that tracks the best bid and ask offers and the quantities of these offers.
struct BBO {
    common::Price bid_price_ = common::PRICE_INVALID, ask_price_ = common::PRICE_INVALID;
    common::Qty bid_qty_ = common::QTY_INVALID, ask_qty_ = common::QTY_INVALID;

    auto ToString() const {
        std::stringstream ss;
        ss << "BBO{" << common::QtyToString(bid_qty_) << "@" << common::PriceToString(bid_price_) << "X"
           << common::PriceToString(ask_price_) << "@" << common::QtyToString(ask_qty_) << "}";

        return ss.str();
    };
};

}  // namespace trading
