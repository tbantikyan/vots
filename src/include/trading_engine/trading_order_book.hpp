/*
 * trading_order_book.hpp
 * Defines the trading engine's order book, the data structure responsible for either tracking the bid and ask orders
 * for a given instrument. Each side is a doubly linked-list of OrdersAtPrice, with the bid side sorted in descending
 * order and the ask side sorted in ascending order. The Order themselves are arranged in FIFO order.
 */

#pragma once

#include "common/types.hpp"
#include "logging/logger.hpp"
#include "market_data/market_update.hpp"
#include "runtime/memory_pool.hpp"
#include "trading_engine/trading_order.hpp"

namespace trading {

class TradingEngine;

class TradingOrderBook final {
   public:
    TradingOrderBook(common::TickerId ticker_id, common::Logger *logger);

    ~TradingOrderBook();

    auto OnMarketUpdate(const exchange::MEMarketUpdate *market_update) noexcept -> void;

    auto SetTradingEngine(TradingEngine *trade_engine) { trade_engine_ = trade_engine; }

    auto UpdateBbo(bool update_bid, bool update_ask) noexcept {
        if (update_bid) {
            if (bids_by_price_ != nullptr) {
                bbo_.bid_price_ = bids_by_price_->price_;
                bbo_.bid_qty_ = bids_by_price_->first_mkt_order_->qty_;
                for (auto order = bids_by_price_->first_mkt_order_->next_order_;
                     order != bids_by_price_->first_mkt_order_; order = order->next_order_) {
                    bbo_.bid_qty_ += order->qty_;
                }
            } else {
                bbo_.bid_price_ = common::PRICE_INVALID;
                bbo_.bid_qty_ = common::QTY_INVALID;
            }
        }

        if (update_ask) {
            if (asks_by_price_ != nullptr) {
                bbo_.ask_price_ = asks_by_price_->price_;
                bbo_.ask_qty_ = asks_by_price_->first_mkt_order_->qty_;
                for (auto order = asks_by_price_->first_mkt_order_->next_order_;
                     order != asks_by_price_->first_mkt_order_; order = order->next_order_) {
                    bbo_.ask_qty_ += order->qty_;
                }
            } else {
                bbo_.ask_price_ = common::PRICE_INVALID;
                bbo_.ask_qty_ = common::QTY_INVALID;
            }
        }
    }

    auto GetBbo() const noexcept -> const BBO * { return &bbo_; }

    auto ToString(bool detailed, bool validity_check) const -> std::string;

    // Deleted default, copy & move constructors and assignment-operators.
    TradingOrderBook() = delete;

    TradingOrderBook(const TradingOrderBook &) = delete;

    TradingOrderBook(const TradingOrderBook &&) = delete;

    auto operator=(const TradingOrderBook &) -> TradingOrderBook & = delete;

    auto operator=(const TradingOrderBook &&) -> TradingOrderBook & = delete;

   private:
    const common::TickerId TICKER_ID;

    TradingEngine *trade_engine_ = nullptr;

    OrderMap oid_to_order_;

    common::MemoryPool<TradingOrdersAtPrice> orders_at_price_pool_;
    TradingOrdersAtPrice *bids_by_price_ = nullptr;
    TradingOrdersAtPrice *asks_by_price_ = nullptr;

    OrdersAtPriceMap price_orders_at_price_;

    common::MemoryPool<TradingOrder> order_pool_;

    BBO bbo_;

    std::string time_str_;
    common::Logger *logger_ = nullptr;

   private:
    auto PriceToIndex(common::Price price) const noexcept { return (price % common::ME_MAX_PRICE_LEVELS); }

    auto GetOrdersAtPrice(common::Price price) const noexcept -> TradingOrdersAtPrice * {
        return price_orders_at_price_.at(PriceToIndex(price));
    }

    auto AddOrdersAtPrice(TradingOrdersAtPrice *new_orders_at_price) noexcept {
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

    auto RemoveOrder(TradingOrder *order) noexcept -> void {
        auto orders_at_price = GetOrdersAtPrice(order->price_);

        if (order->prev_order_ == order) {  // only one element.
            RemoveOrdersAtPrice(order->side_, order->price_);
        } else {  // remove the link.
            const auto order_before = order->prev_order_;
            const auto order_after = order->next_order_;
            order_before->next_order_ = order_after;
            order_after->prev_order_ = order_before;

            if (orders_at_price->first_mkt_order_ == order) {
                orders_at_price->first_mkt_order_ = order_after;
            }

            order->prev_order_ = order->next_order_ = nullptr;
        }

        oid_to_order_.at(order->order_id_) = nullptr;
        order_pool_.Deallocate(order);
    }

    auto AddOrder(TradingOrder *order) noexcept -> void {
        const auto orders_at_price = GetOrdersAtPrice(order->price_);

        if (orders_at_price == nullptr) {
            order->next_order_ = order->prev_order_ = order;

            auto new_orders_at_price =
                orders_at_price_pool_.Allocate(order->side_, order->price_, order, nullptr, nullptr);
            AddOrdersAtPrice(new_orders_at_price);
        } else {
            auto first_order = ((orders_at_price != nullptr) ? orders_at_price->first_mkt_order_ : nullptr);

            first_order->prev_order_->next_order_ = order;
            order->prev_order_ = first_order->prev_order_;
            order->next_order_ = first_order;
            first_order->prev_order_ = order;
        }

        oid_to_order_.at(order->order_id_) = order;
    }
};

using TradingOrderBookMap = std::array<TradingOrderBook *, common::ME_MAX_TICKERS>;
}  // namespace trading
