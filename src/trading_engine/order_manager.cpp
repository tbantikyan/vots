#include "trading_engine/order_manager.hpp"

#include "trading_engine/trading_engine.hpp"

namespace trading {

void OrderManager::NewOrder(OMOrder *order, common::TickerId ticker_id, common::Price price, common::Side side,
                            common::Qty qty) noexcept {
    const exchange::MEClientRequest new_request{.type_ = exchange::ClientRequestType::NEW,
                                                .client_id_ = trading_engine_->ClientId(),
                                                .ticker_id_ = ticker_id,
                                                .order_id_ = next_order_id_,
                                                .side_ = side,
                                                .price_ = price,
                                                .qty_ = qty};
    trading_engine_->SendClientRequest(&new_request);

    *order = {.ticker_id_ = ticker_id,
              .order_id_ = next_order_id_,
              .side_ = side,
              .price_ = price,
              .qty_ = qty,
              .order_state_ = OMOrderState::PENDING_NEW};
    ++next_order_id_;

    logger_->Log("%:% %() % Sent new order % for %\n", __FILE__, __LINE__, __FUNCTION__,
                 common::GetCurrentTimeStr(&time_str_), new_request.ToString().c_str(), order->ToString().c_str());
}

void OrderManager::CancelOrder(OMOrder *order) noexcept {
    const exchange::MEClientRequest cancel_request{.type_ = exchange::ClientRequestType::CANCEL,
                                                   .client_id_ = trading_engine_->ClientId(),
                                                   .ticker_id_ = order->ticker_id_,
                                                   .order_id_ = order->order_id_,
                                                   .side_ = order->side_,
                                                   .price_ = order->price_,
                                                   .qty_ = order->qty_};
    trading_engine_->SendClientRequest(&cancel_request);

    order->order_state_ = OMOrderState::PENDING_CANCEL;

    logger_->Log("%:% %() % Sent cancel % for %\n", __FILE__, __LINE__, __FUNCTION__,
                 common::GetCurrentTimeStr(&time_str_), cancel_request.ToString().c_str(), order->ToString().c_str());
}

}  // namespace trading
