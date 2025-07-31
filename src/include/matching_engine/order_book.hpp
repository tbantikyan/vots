#pragma once

#include "common/types.hpp"
#include "logging/logger.hpp"
#include "market_data/market_update.hpp"
#include "order.hpp"
#include "order_server/client_response.hpp"
#include "runtime/memory_pool.hpp"

namespace exchange {
class MatchingEngine;

class OrderBook final {
   public:
    explicit OrderBook(common::TickerId ticker_id, common::Logger *logger, MatchingEngine *matching_engine);

    ~OrderBook();

    auto Add(common::ClientId client_id, common::OrderId client_order_id, common::TickerId ticker_id, common::Side side,
             common::Price price, common::Qty qty) noexcept -> void;

    auto Cancel(common::ClientId client_id, common::OrderId order_id, common::TickerId ticker_id) noexcept -> void;

    auto ToString(bool detailed, bool validity_check) const -> std::string;

    // Deleted default, copy & move constructors and assignment-operators.
    OrderBook() = delete;

    OrderBook(const OrderBook &) = delete;

    OrderBook(const OrderBook &&) = delete;

    auto operator=(const OrderBook &) -> OrderBook & = delete;

    auto operator=(const OrderBook &&) -> OrderBook & = delete;

   private:
    common::TickerId ticker_id_ = common::TICKER_ID_INVALID;

    MatchingEngine *matching_engine_ = nullptr;

    ClientOrderMap cid_oid_to_order_;

    common::MemoryPool<OrdersAtPrice> orders_at_price_pool_;
    OrdersAtPrice *bids_by_price_ = nullptr;
    OrdersAtPrice *asks_by_price_ = nullptr;

    OrdersAtPriceMap price_orders_at_price_;

    common::MemoryPool<Order> order_pool_;

    MEClientResponse client_response_;
    MEMarketUpdate market_update_;

    common::OrderId next_market_order_id_ = 1;

    std::string time_str_;
    common::Logger *logger_ = nullptr;

   private:
    auto GenerateNewMarketOrderId() noexcept -> common::OrderId { return next_market_order_id_++; }

    auto PriceToIndex(common::Price price) const noexcept { return (price % common::ME_MAX_PRICE_LEVELS); }

    auto GetOrdersAtPrice(common::Price price) const noexcept -> OrdersAtPrice * {
        return price_orders_at_price_.at(PriceToIndex(price));
    }

    auto AddOrdersAtPrice(OrdersAtPrice *new_orders_at_price) noexcept {
        price_orders_at_price_.at(PriceToIndex(new_orders_at_price->price_)) = new_orders_at_price;

        const auto best_orders_by_price =
            (new_orders_at_price->side_ == common::Side::BUY ? bids_by_price_ : asks_by_price_);
        if (best_orders_by_price == nullptr) [[unlikely]] {
            (new_orders_at_price->side_ == common::Side::BUY ? bids_by_price_ : asks_by_price_) = new_orders_at_price;
            new_orders_at_price->prev_entry_ = new_orders_at_price->next_entry_ = new_orders_at_price;
        } else {
            auto target = best_orders_by_price;
            bool add_after =
                ((new_orders_at_price->side_ == common::Side::SELL && new_orders_at_price->price_ > target->price_) ||
                 (new_orders_at_price->side_ == common::Side::BUY && new_orders_at_price->price_ < target->price_));
            if (add_after) {
                target = target->next_entry_;
                add_after =
                    ((new_orders_at_price->side_ == common::Side::SELL &&
                      new_orders_at_price->price_ > target->price_) ||
                     (new_orders_at_price->side_ == common::Side::BUY && new_orders_at_price->price_ < target->price_));
            }
            while (add_after && target != best_orders_by_price) {
                add_after =
                    ((new_orders_at_price->side_ == common::Side::SELL &&
                      new_orders_at_price->price_ > target->price_) ||
                     (new_orders_at_price->side_ == common::Side::BUY && new_orders_at_price->price_ < target->price_));
                if (add_after) {
                    target = target->next_entry_;
                }
            }

            if (add_after) {  // add new_orders_at_price after target.
                if (target == best_orders_by_price) {
                    target = best_orders_by_price->prev_entry_;
                }
                new_orders_at_price->prev_entry_ = target;
                target->next_entry_->prev_entry_ = new_orders_at_price;
                new_orders_at_price->next_entry_ = target->next_entry_;
                target->next_entry_ = new_orders_at_price;
            } else {  // add new_orders_at_price before target.
                new_orders_at_price->prev_entry_ = target->prev_entry_;
                new_orders_at_price->next_entry_ = target;
                target->prev_entry_->next_entry_ = new_orders_at_price;
                target->prev_entry_ = new_orders_at_price;

                if ((new_orders_at_price->side_ == common::Side::BUY &&
                     new_orders_at_price->price_ > best_orders_by_price->price_) ||
                    (new_orders_at_price->side_ == common::Side::SELL &&
                     new_orders_at_price->price_ < best_orders_by_price->price_)) {
                    target->next_entry_ =
                        (target->next_entry_ == best_orders_by_price ? new_orders_at_price : target->next_entry_);
                    (new_orders_at_price->side_ == common::Side::BUY ? bids_by_price_ : asks_by_price_) =
                        new_orders_at_price;
                }
            }
        }
    }

    auto RemoveOrdersAtPrice(common::Side side, common::Price price) noexcept {
        const auto best_orders_by_price = (side == common::Side::BUY ? bids_by_price_ : asks_by_price_);
        auto orders_at_price = GetOrdersAtPrice(price);

        if (orders_at_price->next_entry_ == orders_at_price) [[unlikely]] {  // empty side of book.
            (side == common::Side::BUY ? bids_by_price_ : asks_by_price_) = nullptr;
        } else {
            orders_at_price->prev_entry_->next_entry_ = orders_at_price->next_entry_;
            orders_at_price->next_entry_->prev_entry_ = orders_at_price->prev_entry_;

            if (orders_at_price == best_orders_by_price) {
                (side == common::Side::BUY ? bids_by_price_ : asks_by_price_) = orders_at_price->next_entry_;
            }

            orders_at_price->prev_entry_ = orders_at_price->next_entry_ = nullptr;
        }

        price_orders_at_price_.at(PriceToIndex(price)) = nullptr;

        orders_at_price_pool_.Deallocate(orders_at_price);
    }

    auto GetNextPriority(common::Price price) noexcept {
        const auto orders_at_price = GetOrdersAtPrice(price);
        if (orders_at_price == nullptr) {
            return 1LU;
        }

        return orders_at_price->first_order_->prev_order_->priority_ + 1;
    }

    auto Match(common::TickerId ticker_id, common::ClientId client_id, common::Side side,
               common::OrderId client_order_id, common::OrderId new_market_order_id, Order *bid_itr,
               common::Qty *leaves_qty) noexcept;

    auto CheckForMatch(common::ClientId client_id, common::OrderId client_order_id, common::TickerId ticker_id,
                       common::Side side, common::Price price, common::Qty qty,
                       common::Qty new_market_order_id) noexcept;

    auto RemoveOrder(Order *order) noexcept {
        auto orders_at_price = GetOrdersAtPrice(order->price_);

        if (order->prev_order_ == order) {  // only one element.
            RemoveOrdersAtPrice(order->side_, order->price_);
        } else {  // remove the link.
            const auto order_before = order->prev_order_;
            const auto order_after = order->next_order_;
            order_before->next_order_ = order_after;
            order_after->prev_order_ = order_before;

            if (orders_at_price->first_order_ == order) {
                orders_at_price->first_order_ = order_after;
            }

            order->prev_order_ = order->next_order_ = nullptr;
        }

        cid_oid_to_order_.at(order->client_id_).at(order->client_order_id_) = nullptr;
        order_pool_.Deallocate(order);
    }

    auto AddOrder(Order *order) noexcept {
        const auto orders_at_price = GetOrdersAtPrice(order->price_);

        if (orders_at_price == nullptr) {
            order->next_order_ = order->prev_order_ = order;

            auto new_orders_at_price =
                orders_at_price_pool_.Allocate(order->side_, order->price_, order, nullptr, nullptr);
            AddOrdersAtPrice(new_orders_at_price);
        } else {
            auto first_order = ((orders_at_price != nullptr) ? orders_at_price->first_order_ : nullptr);

            first_order->prev_order_->next_order_ = order;
            order->prev_order_ = first_order->prev_order_;
            order->next_order_ = first_order;
            first_order->prev_order_ = order;
        }

        cid_oid_to_order_.at(order->client_id_).at(order->client_order_id_) = order;
    }
};

using OrderBookMap = std::array<OrderBook *, common::ME_MAX_TICKERS>;

}  // namespace exchange
