#pragma once

#include <array>
#include <sstream>

#include "common/types.hpp"

namespace exchange {

struct Order {
    common::TickerId ticker_id_ = common::TICKER_ID_INVALID;
    common::ClientId client_id_ = common::CLIENT_ID_INVALID;
    common::OrderId client_order_id_ = common::ORDER_ID_INVALID;
    common::OrderId market_order_id_ = common::ORDER_ID_INVALID;
    common::Side side_ = common::Side::INVALID;
    common::Price price_ = common::PRICE_INVALID;
    common::Qty qty_ = common::QTY_INVALID;
    common::Priority priority_ = common::PRIORITY_INVALID;

    Order *prev_order_ = nullptr;
    Order *next_order_ = nullptr;

    // only needed for use with MemPool.
    Order() = default;

    Order(common::TickerId ticker_id, common::ClientId client_id, common::OrderId client_order_id,
          common::OrderId market_order_id, common::Side side, common::Price price, common::Qty qty,
          common::Priority priority, Order *prev_order, Order *next_order) noexcept
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

    auto ToString() const -> std::string;
};

using OrderMap = std::array<Order *, common::ME_MAX_ORDER_IDS>;
using ClientOrderMap = std::array<OrderMap, common::ME_MAX_NUM_CLIENTS>;

struct OrdersAtPrice {
    common::Side side_ = common::Side::INVALID;
    common::Price price_ = common::PRICE_INVALID;

    Order *first_order_ = nullptr;

    OrdersAtPrice *prev_entry_ = nullptr;
    OrdersAtPrice *next_entry_ = nullptr;

    OrdersAtPrice() = default;

    OrdersAtPrice(common::Side side, common::Price price, Order *first_me_order, OrdersAtPrice *prev_entry,
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

using OrdersAtPriceMap = std::array<OrdersAtPrice *, common::ME_MAX_PRICE_LEVELS>;

}  // namespace exchange
