/*
 * client_request.hpp
 * Defines struct representing a client response for the order server to forward to the matching engine and lock-free
 * queue type for the communication channel.
 */

#pragma once

#include <sstream>

#include "common/types.hpp"
#include "runtime/lock_free_queue.hpp"

namespace exchange {

#pragma pack(push, 1)
enum class ClientRequestType : uint8_t { INVALID = 0, NEW = 1, CANCEL = 2 };

inline auto ClientRequestTypeToString(ClientRequestType type) -> std::string {
    switch (type) {
        case ClientRequestType::NEW:
            return "NEW";
        case ClientRequestType::CANCEL:
            return "CANCEL";
        case ClientRequestType::INVALID:
            return "INVALID";
    }
    return "UNKNOWN";
}

struct MEClientRequest {
    ClientRequestType type_ = ClientRequestType::INVALID;

    common::ClientId client_id_ = common::CLIENT_ID_INVALID;
    common::TickerId ticker_id_ = common::TICKER_ID_INVALID;
    common::OrderId order_id_ = common::ORDER_ID_INVALID;
    common::Side side_ = common::Side::INVALID;
    common::Price price_ = common::PRICE_INVALID;
    common::Qty qty_ = common::QTY_INVALID;

    auto ToString() const {
        std::stringstream ss;
        ss << "MEClientRequest"
           << " ["
           << "type:" << ClientRequestTypeToString(type_) << " client:" << common::ClientIdToString(client_id_)
           << " ticker:" << common::TickerIdToString(ticker_id_) << " oid:" << common::OrderIdToString(order_id_)
           << " side:" << common::SideToString(side_) << " qty:" << common::QtyToString(qty_)
           << " price:" << common::PriceToString(price_) << "]";
        return ss.str();
    }
};

#pragma pack(pop)

using ClientRequestLFQueue = common::LockFreeQueue<MEClientRequest>;

}  // namespace wxchange
