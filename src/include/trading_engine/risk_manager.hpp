/*
 * risk_manager.hpp
 * This component calculates trade for the trading engine. Support for checking pre-trade risk, which is whether an
 * order is allowed to be created given the current trading engine and market data.
 */

#pragma once

#include "position_keeper.hpp"

namespace trading {

class OrderManager;

enum class RiskCheckResult : int8_t {
    INVALID = 0,
    ORDER_TOO_LARGE = 1,
    POSITION_TOO_LARGE = 2,
    LOSS_TOO_LARGE = 3,
    ALLOWED = 4
};

inline auto RiskCheckResultToString(RiskCheckResult result) {
    switch (result) {
        case RiskCheckResult::INVALID:
            return "INVALID";
        case RiskCheckResult::ORDER_TOO_LARGE:
            return "ORDER_TOO_LARGE";
        case RiskCheckResult::POSITION_TOO_LARGE:
            return "POSITION_TOO_LARGE";
        case RiskCheckResult::LOSS_TOO_LARGE:
            return "LOSS_TOO_LARGE";
        case RiskCheckResult::ALLOWED:
            return "ALLOWED";
    }

    return "";
}

struct RiskInfo {
    const PositionInfo *position_info_ = nullptr;

    common::RiskCfg risk_cfg_;

    auto CheckPreTradeRisk(common::Side side, common::Qty qty) const noexcept {
        // check order-size
        if (qty > risk_cfg_.max_order_size_) {
            [[unlikely]] return RiskCheckResult::ORDER_TOO_LARGE;
        }
        if (std::abs(position_info_->position_ + (common::SideToValue(side) * static_cast<int32_t>(qty))) >
            static_cast<int32_t>(risk_cfg_.max_position_)) {
            [[unlikely]] return RiskCheckResult::POSITION_TOO_LARGE;
        }
        if (position_info_->total_pnl_ < risk_cfg_.max_loss_) {
            [[unlikely]] return RiskCheckResult::LOSS_TOO_LARGE;
        }

        return RiskCheckResult::ALLOWED;
    }

    auto ToString() const {
        std::stringstream ss;
        ss << "RiskInfo"
           << "["
           << "pos:" << position_info_->ToString() << " " << risk_cfg_.ToString() << "]";

        return ss.str();
    }
};

using TickerRiskInfoHashMap = std::array<RiskInfo, common::ME_MAX_TICKERS>;

class RiskManager {
   public:
    RiskManager(const PositionKeeper *position_keeper, const common::TradeEngineCfgMap &ticker_cfg) {
        for (common::TickerId i = 0; i < common::ME_MAX_TICKERS; ++i) {
            ticker_risk_.at(i).position_info_ = position_keeper->GetPositionInfo(i);
            ticker_risk_.at(i).risk_cfg_ = ticker_cfg[i].risk_cfg_;
        }
    }

    auto CheckPreTradeRisk(common::TickerId ticker_id, common::Side side, common::Qty qty) const noexcept {
        return ticker_risk_.at(ticker_id).CheckPreTradeRisk(side, qty);
    }

    // Deleted default, copy & move constructors and assignment-operators.
    RiskManager() = delete;

    RiskManager(const RiskManager &) = delete;

    RiskManager(const RiskManager &&) = delete;

    auto operator=(const RiskManager &) -> RiskManager & = delete;

    auto operator=(const RiskManager &&) -> RiskManager & = delete;

   private:
    std::string time_str_;

    TickerRiskInfoHashMap ticker_risk_;
};

}  // namespace trading
