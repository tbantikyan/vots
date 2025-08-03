/*
 * client_response.hpp
 * Defines struct representing a client response for the matching engine to forward to the order server and lock-free
 * queue type for the communication channel.
 */

#pragma once

#include <sstream>

#include "common/types.hpp"
#include "runtime/lock_free_queue.hpp"

namespace exchange {

#pragma pack(push, 1)
enum class ClientResponseType : uint8_t { INVALID = 0, ACCEPTED = 1, CANCELED = 2, FILLED = 3, CANCEL_REJECTED = 4 };

inline auto ClientResponseTypeToString(ClientResponseType type) -> std::string {
    switch (type) {
        case ClientResponseType::ACCEPTED:
            return "ACCEPTED";
        case ClientResponseType::CANCELED:
            return "CANCELED";
        case ClientResponseType::FILLED:
            return "FILLED";
        case ClientResponseType::CANCEL_REJECTED:
            return "CANCEL_REJECTED";
        case ClientResponseType::INVALID:
            return "INVALID";
    }
    return "UNKNOWN";
}

struct MEClientResponse {
    ClientResponseType type_ = ClientResponseType::INVALID;
    common::ClientId client_id_ = common::CLIENT_ID_INVALID;
    common::TickerId ticker_id_ = common::TICKER_ID_INVALID;
    common::OrderId client_order_id_ = common::ORDER_ID_INVALID;
    common::OrderId market_order_id_ = common::ORDER_ID_INVALID;
    common::Side side_ = common::Side::INVALID;
    common::Price price_ = common::PRICE_INVALID;
    common::Qty exec_qty_ = common::QTY_INVALID;
    common::Qty leaves_qty_ = common::QTY_INVALID;

    auto ToString() const {
        std::stringstream ss;
        ss << "MEClientResponse"
           << " ["
           << "type:" << ClientResponseTypeToString(type_) << " client:" << common::ClientIdToString(client_id_)
           << " ticker:" << common::TickerIdToString(ticker_id_)
           << " coid:" << common::OrderIdToString(client_order_id_)
           << " moid:" << common::OrderIdToString(market_order_id_) << " side:" << common::SideToString(side_)
           << " exec_qty:" << common::QtyToString(exec_qty_) << " leaves_qty:" << common::QtyToString(leaves_qty_)
           << " price:" << common::PriceToString(price_) << "]";
        return ss.str();
    }
};

// OMClientResponse is the client response type in the public order data protocol.
struct OMClientResponse {
    size_t seq_num_ = 0;
    MEClientResponse me_client_response_;

    auto ToString() const {
        std::stringstream ss;
        ss << "OMClientResponse"
           << " ["
           << "seq:" << seq_num_ << " " << me_client_response_.ToString() << "]";
        return ss.str();
    }
};

#pragma pack(pop)

using ClientResponseLFQueue = common::LockFreeQueue<MEClientResponse>;

}  // namespace exchange
