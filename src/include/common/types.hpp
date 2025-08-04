/*
 * types.hpp
 * Utility type definition for use across vots.
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>

namespace common {

constexpr size_t ME_MAX_TICKERS = 8;

constexpr size_t ME_MAX_CLIENT_UPDATES = 256 * 1024;
constexpr size_t ME_MAX_MARKET_UPDATES = 256 * 1024;

constexpr size_t ME_MAX_NUM_CLIENTS = 256;
constexpr size_t ME_MAX_ORDER_IDS = 1024 * 1024;
constexpr size_t ME_MAX_PRICE_LEVELS = 256;

using OrderId = uint64_t;
constexpr auto ORDER_ID_INVALID = std::numeric_limits<OrderId>::max();

inline auto OrderIdToString(OrderId order_id) -> std::string {
    if (order_id == ORDER_ID_INVALID) [[unlikely]] {
        return "INVALID";
    }

    return std::to_string(order_id);
}

using TickerId = uint32_t;
constexpr auto TICKER_ID_INVALID = std::numeric_limits<TickerId>::max();

inline auto TickerIdToString(TickerId ticker_id) -> std::string {
    if (ticker_id == TICKER_ID_INVALID) [[unlikely]] {
        return "INVALID";
    }

    return std::to_string(ticker_id);
}

using ClientId = uint32_t;
constexpr auto CLIENT_ID_INVALID = std::numeric_limits<ClientId>::max();

inline auto ClientIdToString(ClientId client_id) -> std::string {
    if (client_id == CLIENT_ID_INVALID) [[unlikely]] {
        return "INVALID";
    }

    return std::to_string(client_id);
}

using Price = int64_t;
constexpr auto PRICE_INVALID = std::numeric_limits<Price>::max();

inline auto PriceToString(Price price) -> std::string {
    if (price == PRICE_INVALID) [[unlikely]] {
        return "INVALID";
    }

    return std::to_string(price);
}

using Qty = uint32_t;
constexpr auto QTY_INVALID = std::numeric_limits<Qty>::max();

inline auto QtyToString(Qty qty) -> std::string {
    if (qty == QTY_INVALID) [[unlikely]] {
        return "INVALID";
    }

    return std::to_string(qty);
}

using Priority = uint64_t;
constexpr auto PRIORITY_INVALID = std::numeric_limits<Priority>::max();

inline auto PriorityToString(Priority priority) -> std::string {
    if (priority == PRIORITY_INVALID) [[unlikely]] {
        return "INVALID";
    }

    return std::to_string(priority);
}

enum class Side : int8_t { INVALID = 0, BUY = 1, SELL = -1, MAX = 2 };

inline auto SideToString(Side side) -> std::string {
    switch (side) {
        case Side::BUY:
            return "BUY";
        case Side::SELL:
            return "SELL";
        case Side::INVALID:
            return "INVALID";
        case Side::MAX:
            return "MAX";
    }

    return "UNKNOWN";
}

constexpr auto SideToIndex(Side side) noexcept { return static_cast<size_t>(side) + 1; }

constexpr auto SideToValue(Side side) noexcept { return static_cast<int>(side); }

enum class AlgoType : int8_t { INVALID = 0, RANDOM = 1, MAKER = 2, TAKER = 3, MAX = 4 };

inline auto AlgoTypeToString(AlgoType type) -> std::string {
    switch (type) {
        case AlgoType::RANDOM:
            return "RANDOM";
        case AlgoType::MAKER:
            return "MAKER";
        case AlgoType::TAKER:
            return "TAKER";
        case AlgoType::INVALID:
            return "INVALID";
        case AlgoType::MAX:
            return "MAX";
    }

    return "UNKNOWN";
}

inline auto StringToAlgoType(const std::string &str) -> AlgoType {
    for (auto i = static_cast<int>(AlgoType::INVALID); i <= static_cast<int>(AlgoType::MAX); ++i) {
        const auto algo_type = static_cast<AlgoType>(i);
        if (AlgoTypeToString(algo_type) == str) {
            return algo_type;
        }
    }

    return AlgoType::INVALID;
}

struct RiskCfg {
    Qty max_order_size_ = 0;
    Qty max_position_ = 0;
    double max_loss_ = 0;

    auto ToString() const {
        std::stringstream ss;

        ss << "RiskCfg{"
           << "max-order-size:" << QtyToString(max_order_size_) << " "
           << "max-position:" << QtyToString(max_position_) << " "
           << "max-loss:" << max_loss_ << "}";

        return ss.str();
    }
};

struct TradeEngineCfg {
    Qty clip_ = 0;
    double threshold_ = 0;
    RiskCfg risk_cfg_;

    auto ToString() const {
        std::stringstream ss;
        ss << "TradeEngineCfg{"
           << "clip:" << QtyToString(clip_) << " "
           << "thresh:" << threshold_ << " "
           << "risk:" << risk_cfg_.ToString() << "}";

        return ss.str();
    }
};

using TradeEngineCfgMap = std::array<TradeEngineCfg, ME_MAX_TICKERS>;

}  // namespace common
