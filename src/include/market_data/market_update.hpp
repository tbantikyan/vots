/*
 * market_update.hpp
 * Defines struct representing a market updaete for the matching engine to forward to the market data publisher and
 * lock-free queue type for the communication channel.
 */

#pragma once

#include <sstream>

#include "common/types.hpp"
#include "runtime/lock_free_queue.hpp"

namespace exchange {

#pragma pack(push, 1)
enum class MarketUpdateType : uint8_t { INVALID = 0, ADD = 1, MODIFY = 2, CANCEL = 3, TRADE = 4 };

inline auto MarketUpdateTypeToString(MarketUpdateType type) -> std::string {
    switch (type) {
        case MarketUpdateType::ADD:
            return "ADD";
        case MarketUpdateType::MODIFY:
            return "MODIFY";
        case MarketUpdateType::CANCEL:
            return "CANCEL";
        case MarketUpdateType::TRADE:
            return "TRADE";
        case MarketUpdateType::INVALID:
            return "INVALID";
    }
    return "UNKNOWN";
}

struct MEMarketUpdate {
    MarketUpdateType type_ = MarketUpdateType::INVALID;

    common::OrderId order_id_ = common::ORDER_ID_INVALID;
    common::TickerId ticker_id_ = common::TICKER_ID_INVALID;
    common::Side side_ = common::Side::INVALID;
    common::Price price_ = common::PRICE_INVALID;
    common::Qty qty_ = common::QTY_INVALID;
    common::Priority priority_ = common::PRIORITY_INVALID;

    auto ToString() const {
        std::stringstream ss;
        ss << "MEMarketUpdate"
           << " ["
           << " type:" << MarketUpdateTypeToString(type_) << " ticker:" << common::TickerIdToString(ticker_id_)
           << " oid:" << common::OrderIdToString(order_id_) << " side:" << common::SideToString(side_)
           << " qty:" << common::QtyToString(qty_) << " price:" << common::PriceToString(price_)
           << " priority:" << common::PriorityToString(priority_) << "]";
        return ss.str();
    }
};

#pragma pack(pop)

using MEMarketUpdateLFQueue = common::LockFreeQueue<MEMarketUpdate>;

}  // namespace exchange
