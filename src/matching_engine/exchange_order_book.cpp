#include "matching_engine/exchange_order_book.hpp"

#include "common/perf_utils.hpp"
#include "matching_engine/matching_engine.hpp"

namespace exchange {

ExchangeOrderBook::ExchangeOrderBook(common::TickerId ticker_id, common::Logger *logger,
                                     MatchingEngine *matching_engine)
    : ticker_id_(ticker_id),
      matching_engine_(matching_engine),
      orders_at_price_pool_(common::ME_MAX_PRICE_LEVELS),
      order_pool_(common::ME_MAX_ORDER_IDS),
      logger_(logger) {}

ExchangeOrderBook::~ExchangeOrderBook() {
    logger_->Log("%:% %() % ExchangeOrderBook\n%\n", __FILE__, __LINE__, __FUNCTION__,
                 common::GetCurrentTimeStr(&time_str_), ToString(false, true));

    matching_engine_ = nullptr;
    bids_by_price_ = asks_by_price_ = nullptr;
    for (auto &itr : cid_oid_to_order_) {
        itr.fill(nullptr);
    }
}

auto ExchangeOrderBook::Match(common::TickerId ticker_id, common::ClientId client_id, common::Side side,
                              common::OrderId client_order_id, common::OrderId new_market_order_id, ExchangeOrder *itr,
                              common::Qty *leaves_qty) noexcept {
    const auto order = itr;
    const auto order_qty = order->qty_;
    const auto fill_qty = std::min(*leaves_qty, order_qty);

    *leaves_qty -= fill_qty;
    order->qty_ -= fill_qty;

    client_response_ = {.type_ = ClientResponseType::FILLED,
                        .client_id_ = client_id,
                        .ticker_id_ = ticker_id,
                        .client_order_id_ = client_order_id,
                        .market_order_id_ = new_market_order_id,
                        .side_ = side,
                        .price_ = itr->price_,
                        .exec_qty_ = fill_qty,
                        .leaves_qty_ = *leaves_qty};
    matching_engine_->SendClientResponse(&client_response_);

    client_response_ = {.type_ = ClientResponseType::FILLED,
                        .client_id_ = order->client_id_,
                        .ticker_id_ = ticker_id,
                        .client_order_id_ = order->client_order_id_,
                        .market_order_id_ = order->market_order_id_,
                        .side_ = order->side_,
                        .price_ = itr->price_,
                        .exec_qty_ = fill_qty,
                        .leaves_qty_ = order->qty_};
    matching_engine_->SendClientResponse(&client_response_);

    market_update_ = {.type_ = MarketUpdateType::TRADE,
                      .order_id_ = common::ORDER_ID_INVALID,
                      .ticker_id_ = ticker_id,
                      .side_ = side,
                      .price_ = itr->price_,
                      .qty_ = fill_qty,
                      .priority_ = common::PRIORITY_INVALID};
    matching_engine_->SendMarketUpdate(&market_update_);

    if (order->qty_ == 0) {
        // Upon complete execution, send a CANCEL message as well.
        market_update_ = {.type_ = MarketUpdateType::CANCEL,
                          .order_id_ = order->market_order_id_,
                          .ticker_id_ = ticker_id,
                          .side_ = order->side_,
                          .price_ = order->price_,
                          .qty_ = order_qty,
                          .priority_ = common::PRIORITY_INVALID};
        matching_engine_->SendMarketUpdate(&market_update_);

        START_MEASURE(exchange_me_order_book_remove_order);
        RemoveOrder(order);
        END_MEASURE(exchange_me_order_book_remove_order, (*logger_));
    } else {
        // Upon partial execution, send a MODIFY message as well.
        market_update_ = {.type_ = MarketUpdateType::MODIFY,
                          .order_id_ = order->market_order_id_,
                          .ticker_id_ = ticker_id,
                          .side_ = order->side_,
                          .price_ = order->price_,
                          .qty_ = order->qty_,
                          .priority_ = order->priority_};
        matching_engine_->SendMarketUpdate(&market_update_);
    }
}

auto ExchangeOrderBook::CheckForMatch(common::ClientId client_id, common::OrderId client_order_id,
                                      common::TickerId ticker_id, common::Side side, common::Price price,
                                      common::Qty qty, common::Qty new_market_order_id) noexcept {
    auto leaves_qty = qty;

    if (side == common::Side::BUY) {
        while ((leaves_qty != 0) && (asks_by_price_ != nullptr)) {
            const auto ask_itr = asks_by_price_->first_order_;
            if (price < ask_itr->price_) [[likely]] {
                break;
            }

            START_MEASURE(exchange_me_order_book_match);
            Match(ticker_id, client_id, side, client_order_id, new_market_order_id, ask_itr, &leaves_qty);
            END_MEASURE(exchange_me_order_book_match, (*logger_));
        }
    }
    if (side == common::Side::SELL) {
        while ((leaves_qty != 0) && (bids_by_price_ != nullptr)) {
            const auto bid_itr = bids_by_price_->first_order_;
            if (price > bid_itr->price_) [[likely]] {
                break;
            }

            START_MEASURE(exchange_me_order_book_match);
            Match(ticker_id, client_id, side, client_order_id, new_market_order_id, bid_itr, &leaves_qty);
            END_MEASURE(exchange_me_order_book_match, (*logger_));
        }
    }

    return leaves_qty;
}

void ExchangeOrderBook::Add(common::ClientId client_id, common::OrderId client_order_id, common::TickerId ticker_id,
                            common::Side side, common::Price price, common::Qty qty) noexcept {
    const auto new_market_order_id = GenerateNewMarketOrderId();
    client_response_ = {.type_ = ClientResponseType::ACCEPTED,
                        .client_id_ = client_id,
                        .ticker_id_ = ticker_id,
                        .client_order_id_ = client_order_id,
                        .market_order_id_ = new_market_order_id,
                        .side_ = side,
                        .price_ = price,
                        .exec_qty_ = 0,
                        .leaves_qty_ = qty};
    matching_engine_->SendClientResponse(&client_response_);

    START_MEASURE(exchange_me_order_book_check_for_match);
    const auto leaves_qty = CheckForMatch(client_id, client_order_id, ticker_id, side, price, qty, new_market_order_id);
    END_MEASURE(exchange_me_order_book_check_for_match, (*logger_));

    if (leaves_qty != 0) [[likely]] {
        const auto priority = GetNextPriority(price);

        auto order = order_pool_.Allocate(ticker_id, client_id, client_order_id, new_market_order_id, side, price,
                                          leaves_qty, priority, nullptr, nullptr);
        START_MEASURE(exchange_me_order_book_add_order);
        AddOrder(order);
        END_MEASURE(exchange_me_order_book_add_order, (*logger_));

        market_update_ = {.type_ = MarketUpdateType::ADD,
                          .order_id_ = new_market_order_id,
                          .ticker_id_ = ticker_id,
                          .side_ = side,
                          .price_ = price,
                          .qty_ = leaves_qty,
                          .priority_ = priority};
        matching_engine_->SendMarketUpdate(&market_update_);
    }
}

void ExchangeOrderBook::Cancel(common::ClientId client_id, common::OrderId order_id,
                               common::TickerId ticker_id) noexcept {
    auto is_cancelable = (client_id < cid_oid_to_order_.size());
    ExchangeOrder *exchange_order = nullptr;
    if (is_cancelable) [[likely]] {
        auto &co_itr = cid_oid_to_order_.at(client_id);
        exchange_order = co_itr.at(order_id);
        is_cancelable = (exchange_order != nullptr);
    }

    if (!is_cancelable) [[unlikely]] {
        client_response_ = {.type_ = ClientResponseType::CANCEL_REJECTED,
                            .client_id_ = client_id,
                            .ticker_id_ = ticker_id,
                            .client_order_id_ = order_id,
                            .market_order_id_ = common::ORDER_ID_INVALID,
                            .side_ = common::Side::INVALID,
                            .price_ = common::PRICE_INVALID,
                            .exec_qty_ = common::QTY_INVALID,
                            .leaves_qty_ = common::QTY_INVALID};
    } else {
        client_response_ = {.type_ = ClientResponseType::CANCELED,
                            .client_id_ = client_id,
                            .ticker_id_ = ticker_id,
                            .client_order_id_ = order_id,
                            .market_order_id_ = exchange_order->market_order_id_,
                            .side_ = exchange_order->side_,
                            .price_ = exchange_order->price_,
                            .exec_qty_ = common::QTY_INVALID,
                            .leaves_qty_ = exchange_order->qty_};
        market_update_ = {.type_ = MarketUpdateType::CANCEL,
                          .order_id_ = exchange_order->market_order_id_,
                          .ticker_id_ = ticker_id,
                          .side_ = exchange_order->side_,
                          .price_ = exchange_order->price_,
                          .qty_ = 0,
                          .priority_ = exchange_order->priority_};

        START_MEASURE(exchange_me_order_book_remove_order);
        RemoveOrder(exchange_order);
        END_MEASURE(exchange_me_order_book_remove_order, (*logger_));

        matching_engine_->SendMarketUpdate(&market_update_);
    }

    matching_engine_->SendClientResponse(&client_response_);
}

auto ExchangeOrderBook::ToString(bool detailed, bool validity_check) const -> std::string {
    std::stringstream ss;

    auto printer = [&](std::stringstream &ss, OrdersAtPrice *itr, common::Side side, common::Price &last_price,
                       bool sanity_check) {
        char buf[4096];
        common::Qty qty = 0;
        size_t num_orders = 0;

        for (auto o_itr = itr->first_order_;; o_itr = o_itr->next_order_) {
            qty += o_itr->qty_;
            ++num_orders;
            if (o_itr->next_order_ == itr->first_order_) {
                break;
            }
        }
        sprintf(buf, " <px:%3s p:%3s n:%3s> %-3s @ %-5s(%-4s)", common::PriceToString(itr->price_).c_str(),
                common::PriceToString(itr->prev_entry_->price_).c_str(),
                common::PriceToString(itr->next_entry_->price_).c_str(), common::PriceToString(itr->price_).c_str(),
                common::QtyToString(qty).c_str(), std::to_string(num_orders).c_str());
        ss << buf;
        for (auto o_itr = itr->first_order_;; o_itr = o_itr->next_order_) {
            if (detailed) {
                sprintf(buf, "[oid:%s q:%s p:%s n:%s] ", common::OrderIdToString(o_itr->market_order_id_).c_str(),
                        common::QtyToString(o_itr->qty_).c_str(),
                        common::OrderIdToString(o_itr->prev_order_ ? o_itr->prev_order_->market_order_id_
                                                                   : common::ORDER_ID_INVALID)
                            .c_str(),
                        common::OrderIdToString(o_itr->next_order_ ? o_itr->next_order_->market_order_id_
                                                                   : common::ORDER_ID_INVALID)
                            .c_str());
                ss << buf;
            }
            if (o_itr->next_order_ == itr->first_order_) {
                break;
            }
        }

        ss << std::endl;  // NOLINT

        if (sanity_check) {
            if ((side == common::Side::SELL && last_price >= itr->price_) ||
                (side == common::Side::BUY && last_price <= itr->price_)) {
                FATAL("Bids/Asks not sorted by ascending/descending prices last:" + common::PriceToString(last_price) +
                      " itr:" + itr->ToString());
            }
            last_price = itr->price_;
        }
    };

    ss << "Ticker:" << common::TickerIdToString(ticker_id_) << std::endl;  // NOLINT
    {
        auto ask_itr = asks_by_price_;
        auto last_ask_price = std::numeric_limits<common::Price>::min();
        for (size_t count = 0; ask_itr != nullptr; ++count) {
            ss << "ASKS L:" << count << " => ";
            auto next_ask_itr = (ask_itr->next_entry_ == asks_by_price_ ? nullptr : ask_itr->next_entry_);
            printer(ss, ask_itr, common::Side::SELL, last_ask_price, validity_check);
            ask_itr = next_ask_itr;
        }
    }

    ss << std::endl << "                          X" << std::endl << std::endl;  // NOLINT

    {
        auto bid_itr = bids_by_price_;
        auto last_bid_price = std::numeric_limits<common::Price>::max();
        for (size_t count = 0; bid_itr != nullptr; ++count) {
            ss << "BIDS L:" << count << " => ";
            auto next_bid_itr = (bid_itr->next_entry_ == bids_by_price_ ? nullptr : bid_itr->next_entry_);
            printer(ss, bid_itr, common::Side::BUY, last_bid_price, validity_check);
            bid_itr = next_bid_itr;
        }
    }

    return ss.str();
}

}  // namespace exchange
