/*
 * position_keeper.hpp
 * Defines PositionKeeper and low-level struct PositionInfo. PositionInfo tracks position (long or short), realized,
 * unrealized, & total PnL (profits and loss), and Volume Weighted Average Price (VWAP) for an instrument.
 * PositionKeeper manages PositionInfo objects.
 */

#pragma once

#include "common/types.hpp"
#include "logging/logger.hpp"
#include "order_gateway/client_response.hpp"
#include "trading_engine/trading_order.hpp"

namespace trading {

struct PositionInfo {
    int32_t position_ = 0;
    double real_pnl_ = 0, unreal_pnl_ = 0, total_pnl_ = 0;
    std::array<double, SideToIndex(common::Side::MAX) + 1> open_vwap_;
    common::Qty volume_ = 0;
    const BBO *bbo_ = nullptr;

    auto ToString() const {
        std::stringstream ss;
        ss << "Position{"
           << "pos:" << position_ << " u-pnl:" << unreal_pnl_ << " r-pnl:" << real_pnl_ << " t-pnl:" << total_pnl_
           << " vol:" << common::QtyToString(volume_) << " vwaps:["
           << ((position_ != 0) ? open_vwap_.at(SideToIndex(common::Side::BUY)) / std::abs(position_) : 0) << "X"
           << ((position_ != 0) ? open_vwap_.at(SideToIndex(common::Side::SELL)) / std::abs(position_) : 0) << "] "
           << ((bbo_ != nullptr) ? bbo_->ToString() : "") << "}";

        return ss.str();
    }

    auto AddFill(const exchange::MEClientResponse *client_response, common::Logger *logger) noexcept {
        const auto old_position = position_;
        const auto side_index = SideToIndex(client_response->side_);
        const auto opp_side_index =
            SideToIndex(client_response->side_ == common::Side::BUY ? common::Side::SELL : common::Side::BUY);
        const auto side_value = SideToValue(client_response->side_);
        position_ += client_response->exec_qty_ * side_value;
        volume_ += client_response->exec_qty_;

        if (old_position * SideToValue(client_response->side_) >= 0) {  // opened / increased position.
            open_vwap_[side_index] += (client_response->price_ * client_response->exec_qty_);
        } else {  // decreased position.
            const auto opp_side_vwap = open_vwap_[opp_side_index] / std::abs(old_position);
            open_vwap_[opp_side_index] = opp_side_vwap * std::abs(position_);
            real_pnl_ += std::min(static_cast<int32_t>(client_response->exec_qty_), std::abs(old_position)) *
                         (opp_side_vwap - client_response->price_) * SideToValue(client_response->side_);
            if (position_ * old_position < 0) {  // flipped position to opposite sign.
                open_vwap_[side_index] = (client_response->price_ * std::abs(position_));
                open_vwap_[opp_side_index] = 0;
            }
        }

        if (position_ == 0) {  // flat
            open_vwap_[SideToIndex(common::Side::BUY)] = open_vwap_[SideToIndex(common::Side::SELL)] = 0;
            unreal_pnl_ = 0;
        } else {
            if (position_ > 0) {
                unreal_pnl_ =
                    (client_response->price_ - open_vwap_[SideToIndex(common::Side::BUY)] / std::abs(position_)) *
                    std::abs(position_);
            } else {
                unreal_pnl_ =
                    (open_vwap_[SideToIndex(common::Side::SELL)] / std::abs(position_) - client_response->price_) *
                    std::abs(position_);
            }
        }

        total_pnl_ = unreal_pnl_ + real_pnl_;

        std::string time_str;
        logger->Log("%:% %() % % %\n", __FILE__, __LINE__, __FUNCTION__, common::GetCurrentTimeStr(&time_str),
                    ToString(), client_response->ToString().c_str());
    }

    auto UpdateBbo(const BBO *bbo, common::Logger *logger) noexcept {
        std::string time_str;
        bbo_ = bbo;

        if ((position_ != 0) && bbo->bid_price_ != common::PRICE_INVALID && bbo->ask_price_ != common::PRICE_INVALID) {
            const auto mid_price = (bbo->bid_price_ + bbo->ask_price_) * 0.5;
            if (position_ > 0) {
                unreal_pnl_ = (mid_price - open_vwap_[common::SideToIndex(common::Side::BUY)] / std::abs(position_)) *
                              std::abs(position_);
            } else {
                unreal_pnl_ = (open_vwap_[common::SideToIndex(common::Side::SELL)] / std::abs(position_) - mid_price) *
                              std::abs(position_);
            }

            const auto old_total_pnl = total_pnl_;
            total_pnl_ = unreal_pnl_ + real_pnl_;

            if (total_pnl_ != old_total_pnl) {
                logger->Log("%:% %() % % %\n", __FILE__, __LINE__, __FUNCTION__, common::GetCurrentTimeStr(&time_str),
                            ToString(), bbo_->ToString());
            }
        }
    }
};

class PositionKeeper {
   public:
    explicit PositionKeeper(common::Logger *logger) : logger_(logger) {}

    // Deleted default, copy & move constructors and assignment-operators.
    PositionKeeper() = delete;

    PositionKeeper(const PositionKeeper &) = delete;

    PositionKeeper(const PositionKeeper &&) = delete;

    auto operator=(const PositionKeeper &) -> PositionKeeper & = delete;

    auto operator=(const PositionKeeper &&) -> PositionKeeper & = delete;

   private:
    std::string time_str_;
    common::Logger *logger_ = nullptr;

    std::array<PositionInfo, common::ME_MAX_TICKERS> ticker_position_;

   public:
    auto AddFill(const exchange::MEClientResponse *client_response) noexcept {
        ticker_position_.at(client_response->ticker_id_).AddFill(client_response, logger_);
    }

    auto UpdateBbo(common::TickerId ticker_id, const BBO *bbo) noexcept {
        ticker_position_.at(ticker_id).UpdateBbo(bbo, logger_);
    }

    auto GetPositionInfo(common::TickerId ticker_id) const noexcept { return &(ticker_position_.at(ticker_id)); }

    auto ToString() const {
        double total_pnl = 0;
        common::Qty total_vol = 0;

        std::stringstream ss;
        for (common::TickerId i = 0; i < ticker_position_.size(); ++i) {
            ss << "TickerId:" << common::TickerIdToString(i) << " " << ticker_position_.at(i).ToString() << "\n";

            total_pnl += ticker_position_.at(i).total_pnl_;
            total_vol += ticker_position_.at(i).volume_;
        }
        ss << "Total PnL:" << total_pnl << " Vol:" << total_vol << "\n";

        return ss.str();
    }
};

}  // namespace trading
