/*
 * market_data_consumer.hpp
 * Define the market participant component responsible for subscribing to the trading exchange public data streams.
 * Subscribes to the snapshot stream only if the market participant becomes unsynchronized until synchronization.
 */

#pragma once

#include <functional>
#include <map>

#include "common/integrity.hpp"
#include "market_update.hpp"
#include "network/mcast_socket.hpp"
#include "runtime/lock_free_queue.hpp"
#include "runtime/threads.hpp"

namespace trading {

class MarketDataConsumer {
   public:
    MarketDataConsumer(common::ClientId client_id, exchange::MEMarketUpdateLFQueue *market_updates,
                       const std::string &iface, const std::string &snapshot_ip, int snapshot_port,
                       const std::string &incremental_ip, int incremental_port);

    ~MarketDataConsumer() {
        Stop();

        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(5s);
    }

    void Start() {
        run_ = true;
        ASSERT(common::CreateAndStartThread(-1, "Trading/MarketDataConsumer", [this]() { Run(); }) != nullptr,
               "Failed to start MarketData thread.");
    }

    void Stop() { run_ = false; }

    // Deleted default, copy & move constructors and assignment-operators.
    MarketDataConsumer() = delete;

    MarketDataConsumer(const MarketDataConsumer &) = delete;

    MarketDataConsumer(const MarketDataConsumer &&) = delete;

    auto operator=(const MarketDataConsumer &) -> MarketDataConsumer & = delete;

    auto operator=(const MarketDataConsumer &&) -> MarketDataConsumer & = delete;

   private:
    size_t next_exp_inc_seq_num_ = 1;
    exchange::MEMarketUpdateLFQueue *incoming_md_updates_ = nullptr;

    volatile bool run_ = false;

    std::string time_str_;
    common::Logger logger_;
    common::McastSocket incremental_mcast_socket_, snapshot_mcast_socket_;

    bool in_recovery_ = false;  // Indicates whether currently trying to synchronize.
    const std::string IFACE, SNAPSHOT_IP;
    const int SNAPSHOT_PORT;

    /*
     * During synchronization effort, stores received message in order fashion. Though std::map is inefficiently
     * implemented, the market participant cannot trade with an unsynchronized orderbook anyway. Moreover, the latency
     * is throttled by the relatively slow speed of the snapshot stream.
     */
    using QueuedMarketUpdates = std::map<size_t, exchange::MEMarketUpdate>;
    QueuedMarketUpdates snapshot_queued_msgs_, incremental_queued_msgs_;

   private:
    void Run() noexcept;

    void RecvCallback(common::McastSocket *socket) noexcept;

    auto QueueMessage(bool is_snapshot, const exchange::MDPMarketUpdate *request);

    void StartSnapshotSync();
    void CheckSnapshotSync();
};

}  // namespace trading
