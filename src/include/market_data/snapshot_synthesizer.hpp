/*
 * snapshot_synthesizer.hpp
 * Aggregates messages from the matching engine into snapshots that are occasionally pushed via the multicast snapshot
 * stream for market participants to synchronize their data with the trading exchange. Runs in its own thread as the
 * market data publisher needs to achieve very low latency for the incremental stream.
 */

#pragma once

#include "common/integrity.hpp"
#include "common/types.hpp"
#include "logging/logger.hpp"
#include "market_update.hpp"
#include "matching_engine/exchange_order.hpp"
#include "network/mcast_socket.hpp"
#include "runtime/lock_free_queue.hpp"
#include "runtime/memory_pool.hpp"
#include "runtime/threads.hpp"

namespace exchange {

class SnapshotSynthesizer {
   public:
    SnapshotSynthesizer(MDPMarketUpdateLFQueue *market_updates, const std::string &iface,
                        const std::string &snapshot_ip, int snapshot_port);

    ~SnapshotSynthesizer();

    void Start();

    void Stop();

    auto AddToSnapshot(const MDPMarketUpdate *market_update);

    auto PublishSnapshot();

    void Run();

    // Deleted default, copy & move constructors and assignment-operators.
    SnapshotSynthesizer() = delete;

    SnapshotSynthesizer(const SnapshotSynthesizer &) = delete;

    SnapshotSynthesizer(const SnapshotSynthesizer &&) = delete;

    auto operator=(const SnapshotSynthesizer &) -> SnapshotSynthesizer & = delete;

    auto operator=(const SnapshotSynthesizer &&) -> SnapshotSynthesizer & = delete;

   private:
    MDPMarketUpdateLFQueue *snapshot_md_updates_ = nullptr;

    common::Logger logger_;

    volatile bool run_ = false;

    std::string time_str_;

    common::McastSocket snapshot_socket_;

    std::array<std::array<MEMarketUpdate *, common::ME_MAX_ORDER_IDS>, common::ME_MAX_TICKERS> ticker_orders_;
    size_t last_inc_seq_num_ = 0;
    common::Nanos last_snapshot_time_ = 0;

    common::MemoryPool<MEMarketUpdate> order_pool_;
};

}  // namespace exchange
