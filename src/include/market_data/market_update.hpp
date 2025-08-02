/*
 * market_update.hpp
 * Defines struct representing a market update for the matching engine to forward to the market data publisher and
 * lock-free queue type for the communication channel.
 */

#pragma once

#include <sstream>

#include "common/types.hpp"
#include "runtime/lock_free_queue.hpp"

namespace exchange {

#pragma pack(push, 1)
enum class MarketUpdateType : uint8_t {
    INVALID = 0,
    CLEAR = 1,
    ADD = 2,
    MODIFY = 3,
    CANCEL = 4,
    TRADE = 5,
    SNAPSHOT_START = 6,
    SNAPSHOT_END = 7
};

inline auto MarketUpdateTypeToString(MarketUpdateType type) -> std::string {
    switch (type) {
        case MarketUpdateType::CLEAR:
            return "CLEAR";
        case MarketUpdateType::ADD:
            return "ADD";
        case MarketUpdateType::MODIFY:
            return "MODIFY";
        case MarketUpdateType::CANCEL:
            return "CANCEL";
        case MarketUpdateType::TRADE:
            return "TRADE";
        case MarketUpdateType::SNAPSHOT_START:
            return "SNAPSHOT_START";
        case MarketUpdateType::SNAPSHOT_END:
            return "SNAPSHOT_END";
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

// MDPMarketUpdate is the type of an incremental market update in the public market data protocol.
struct MDPMarketUpdate {
    size_t seq_num_ = 0;
    MEMarketUpdate me_market_update_;

    auto ToString() const {
        std::stringstream ss;
        ss << "MDPMarketUpdate"
           << " ["
           << " seq:" << seq_num_ << " " << me_market_update_.ToString() << "]";
        return ss.str();
    }
};
#pragma pack(pop)

using MEMarketUpdateLFQueue = common::LockFreeQueue<MEMarketUpdate>;
using MDPMarketUpdateLFQueue = common::LockFreeQueue<MDPMarketUpdate>;

}  // namespace exchange
