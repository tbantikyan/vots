/*
 * types.hpp
 * Utility type definition for use across vots.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
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

enum class Side : int8_t { INVALID = 0, BUY = 1, SELL = -1 };

inline auto SideToString(Side side) -> std::string {
    switch (side) {
        case Side::BUY:
            return "BUY";
        case Side::SELL:
            return "SELL";
        case Side::INVALID:
            return "INVALID";
    }

    return "UNKNOWN";
}

}  // namespace common
