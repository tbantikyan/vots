/*
 * market_data_publisher.hpp
 * The MarketDataPublisher is responsible for publishing public market updates for market participants to follow along
 * with trade executions and order modifications. It publishes all updates to on a UDP multicast socket called the
 * incremental stream; UDP helps achieve the ultra low-latency need of sharing updates with participants as quickly as
 * possible. To allow participants to synchronize with the trading exchange, the market data publisher also compiles
 * data and occasionally pushes a large snapshot via the snapshot stream.
 */

#pragma once

#include <functional>

#include "snapshot_synthesizer.hpp"

namespace exchange {
class MarketDataPublisher {
   public:
    MarketDataPublisher(MEMarketUpdateLFQueue *market_updates, const std::string &iface, const std::string &snapshot_ip,
                        int snapshot_port, const std::string &incremental_ip, int incremental_port);

    ~MarketDataPublisher() {
        Stop();

        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(5s);

        delete snapshot_synthesizer_;
        snapshot_synthesizer_ = nullptr;
    }

    void Run() noexcept;

    void Start() {
        run_ = true;

        ASSERT(common::CreateAndStartThread(-1, "exchange/MarketDataPublisher", [this]() { Run(); }) != nullptr,
               "Failed to start MarketData thread.");

        snapshot_synthesizer_->Start();
    }

    void Stop() {
        run_ = false;

        snapshot_synthesizer_->Stop();
    }

    // Deleted default, copy & move constructors and assignment-operators.
    MarketDataPublisher() = delete;

    MarketDataPublisher(const MarketDataPublisher &) = delete;

    MarketDataPublisher(const MarketDataPublisher &&) = delete;

    auto operator=(const MarketDataPublisher &) -> MarketDataPublisher & = delete;

    auto operator=(const MarketDataPublisher &&) -> MarketDataPublisher & = delete;

   private:
    size_t next_inc_seq_num_ = 1;
    MEMarketUpdateLFQueue *outgoing_md_updates_ = nullptr;

    MDPMarketUpdateLFQueue snapshot_md_updates_;

    volatile bool run_ = false;

    std::string time_str_;

    common::Logger logger_;

    common::McastSocket incremental_socket_;

    SnapshotSynthesizer *snapshot_synthesizer_ = nullptr;
};

}  // namespace exchange
