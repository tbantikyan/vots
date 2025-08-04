#include "trading_engine/trading_order_book.hpp"

#include "trading_engine/trading_engine.hpp"

namespace trading {

TradingOrderBook::TradingOrderBook(common::TickerId ticker_id, common::Logger *logger)
    : TICKER_ID(ticker_id),
      orders_at_price_pool_(common::ME_MAX_PRICE_LEVELS),
      order_pool_(common::ME_MAX_ORDER_IDS),
      logger_(logger) {}

TradingOrderBook::~TradingOrderBook() {
    logger_->Log("%:% %() % OrderBook\n%\n", __FILE__, __LINE__, __FUNCTION__, common::GetCurrentTimeStr(&time_str_),
                 ToString(false, true));

    trade_engine_ = nullptr;
    bids_by_price_ = asks_by_price_ = nullptr;
    oid_to_order_.fill(nullptr);
}

// Process market data update and update the limit order book.
void TradingOrderBook::OnMarketUpdate(const exchange::MEMarketUpdate *market_update) noexcept {
    const auto bid_updated = ((bids_by_price_ != nullptr) && market_update->side_ == common::Side::BUY &&
                              market_update->price_ >= bids_by_price_->price_);
    const auto ask_updated = ((asks_by_price_ != nullptr) && market_update->side_ == common::Side::SELL &&
                              market_update->price_ <= asks_by_price_->price_);

    switch (market_update->type_) {
        case exchange::MarketUpdateType::ADD: {
            auto order = order_pool_.Allocate(market_update->order_id_, market_update->side_, market_update->price_,
                                              market_update->qty_, market_update->priority_, nullptr, nullptr);
            AddOrder(order);
        } break;
        case exchange::MarketUpdateType::MODIFY: {
            auto order = oid_to_order_.at(market_update->order_id_);
            order->qty_ = market_update->qty_;
        } break;
        case exchange::MarketUpdateType::CANCEL: {
            auto order = oid_to_order_.at(market_update->order_id_);
            RemoveOrder(order);
        } break;
        case exchange::MarketUpdateType::TRADE: {
            trade_engine_->OnTradeUpdate(market_update, this);
            return;
        } break;
        case exchange::MarketUpdateType::CLEAR: {  // Clear the full limit order book and deallocate
                                                   // TradingOrdersAtPrice and TradingOrder objects.
            for (auto &order : oid_to_order_) {
                if (order != nullptr) {
                    order_pool_.Deallocate(order);
                }
            }
            oid_to_order_.fill(nullptr);

            if (bids_by_price_ != nullptr) {
                for (auto bid = bids_by_price_->next_entry_; bid != bids_by_price_; bid = bid->next_entry_) {
                    orders_at_price_pool_.Deallocate(bid);
                }
                orders_at_price_pool_.Deallocate(bids_by_price_);
            }

            if (asks_by_price_ != nullptr) {
                for (auto ask = asks_by_price_->next_entry_; ask != asks_by_price_; ask = ask->next_entry_) {
                    orders_at_price_pool_.Deallocate(ask);
                }
                orders_at_price_pool_.Deallocate(asks_by_price_);
            }

            bids_by_price_ = asks_by_price_ = nullptr;
        } break;
        case exchange::MarketUpdateType::INVALID:
        case exchange::MarketUpdateType::SNAPSHOT_START:
        case exchange::MarketUpdateType::SNAPSHOT_END:
            break;
    }

    UpdateBbo(bid_updated, ask_updated);

    logger_->Log("%:% %() % % %", __FILE__, __LINE__, __FUNCTION__, common::GetCurrentTimeStr(&time_str_),
                 market_update->ToString(), bbo_.ToString());

    trade_engine_->OnOrderBookUpdate(market_update->ticker_id_, market_update->price_, market_update->side_, this);
}

auto TradingOrderBook::ToString(bool detailed, bool validity_check) const -> std::string {
    std::stringstream ss;

    auto printer = [&](std::stringstream &ss, TradingOrdersAtPrice *itr, common::Side side, common::Price &last_price,
                       bool sanity_check) {
        char buf[4096];
        common::Qty qty = 0;
        size_t num_orders = 0;

        for (auto o_itr = itr->first_mkt_order_;; o_itr = o_itr->next_order_) {
            qty += o_itr->qty_;
            ++num_orders;
            if (o_itr->next_order_ == itr->first_mkt_order_) {
                break;
            }
        }
        sprintf(buf, " <px:%3s p:%3s n:%3s> %-3s @ %-5s(%-4s)", common::PriceToString(itr->price_).c_str(),
                common::PriceToString(itr->prev_entry_->price_).c_str(),
                common::PriceToString(itr->next_entry_->price_).c_str(), common::PriceToString(itr->price_).c_str(),
                common::QtyToString(qty).c_str(), std::to_string(num_orders).c_str());
        ss << buf;
        for (auto o_itr = itr->first_mkt_order_;; o_itr = o_itr->next_order_) {
            if (detailed) {
                sprintf(buf, "[oid:%s q:%s p:%s n:%s] ", common::OrderIdToString(o_itr->order_id_).c_str(),
                        common::QtyToString(o_itr->qty_).c_str(),
                        common::OrderIdToString(o_itr->prev_order_ ? o_itr->prev_order_->order_id_
                                                                   : common::ORDER_ID_INVALID)
                            .c_str(),
                        common::OrderIdToString(o_itr->next_order_ ? o_itr->next_order_->order_id_
                                                                   : common::ORDER_ID_INVALID)
                            .c_str());
                ss << buf;
            }
            if (o_itr->next_order_ == itr->first_mkt_order_) {
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

    ss << "Ticker:" << common::TickerIdToString(TICKER_ID) << std::endl;  // NOLINT
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

}  // namespace trading
