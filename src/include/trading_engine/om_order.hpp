/*
 * om_order.hpp
 * Represents an order managed by the OrderManager.
 */

#pragma once

#include <array>
#include <sstream>

#include "common/types.hpp"

namespace trading {

enum class OMOrderState : int8_t { INVALID = 0, PENDING_NEW = 1, LIVE = 2, PENDING_CANCEL = 3, DEAD = 4 };

inline auto OMOrderStateToString(OMOrderState side) -> std::string {
    switch (side) {
        case OMOrderState::PENDING_NEW:
            return "PENDING_NEW";
        case OMOrderState::LIVE:
            return "LIVE";
        case OMOrderState::PENDING_CANCEL:
            return "PENDING_CANCEL";
        case OMOrderState::DEAD:
            return "DEAD";
        case OMOrderState::INVALID:
            return "INVALID";
    }

    return "UNKNOWN";
}

struct OMOrder {
    common::TickerId ticker_id_ = common::TICKER_ID_INVALID;
    common::OrderId order_id_ = common::ORDER_ID_INVALID;
    common::Side side_ = common::Side::INVALID;
    common::Price price_ = common::PRICE_INVALID;
    common::Qty qty_ = common::QTY_INVALID;
    OMOrderState order_state_ = OMOrderState::INVALID;

    auto ToString() const {
        std::stringstream ss;
        ss << "OMOrder"
           << "["
           << "tid:" << common::TickerIdToString(ticker_id_) << " "
           << "oid:" << common::OrderIdToString(order_id_) << " "
           << "side:" << common::SideToString(side_) << " "
           << "price:" << common::PriceToString(price_) << " "
           << "qty:" << common::QtyToString(qty_) << " "
           << "state:" << OMOrderStateToString(order_state_) << "]";

        return ss.str();
    }
};

using OMOrderSideMap = std::array<OMOrder, common::SideToIndex(common::Side::MAX) + 1>;
using OMOrderTickerSideMap = std::array<OMOrderSideMap, common::ME_MAX_TICKERS>;

}  // namespace trading
