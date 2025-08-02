#include "market_data/market_data_consumer.hpp"

namespace trading {
MarketDataConsumer::MarketDataConsumer(common::ClientId client_id, exchange::MEMarketUpdateLFQueue *market_updates,
                                       const std::string &iface,
                                       const std::string &snapshot_ip,  // NOLINT
                                       int snapshot_port, const std::string &incremental_ip, int incremental_port)
    : incoming_md_updates_(market_updates),
      run_(false),  // NOLINT
      logger_("trading_market_data_consumer_" + std::to_string(client_id) + ".log"),
      incremental_mcast_socket_(logger_),
      snapshot_mcast_socket_(logger_),
      IFACE(iface),
      SNAPSHOT_IP(snapshot_ip),
      SNAPSHOT_PORT(snapshot_port) {
    auto recv_callback = [this](auto socket) { RecvCallback(socket); };

    incremental_mcast_socket_.recv_callback_ = recv_callback;
    ASSERT(incremental_mcast_socket_.Init(incremental_ip, iface, incremental_port, /*is_listening*/ true) >= 0,
           "Unable to create incremental mcast socket. error:" + std::string(std::strerror(errno)));

    ASSERT(incremental_mcast_socket_.Join(incremental_ip),
           "Join failed on:" + std::to_string(incremental_mcast_socket_.socket_fd_) +
               " error:" + std::string(std::strerror(errno)));

    snapshot_mcast_socket_.recv_callback_ = recv_callback;
}

// Main loop for this thread - reads and processes messages from the multicast sockets - the heavy lifting is in the
// recvCallback() and checkSnapshotSync() methods.
auto MarketDataConsumer::Run() noexcept -> void {
    logger_.Log("%:% %() %\n", __FILE__, __LINE__, __FUNCTION__, common::GetCurrentTimeStr(&time_str_));
    while (run_) {
        incremental_mcast_socket_.SendAndRecv();
        snapshot_mcast_socket_.SendAndRecv();
    }
}

// Start the process of snapshot synchronization by subscribing to the snapshot multicast stream.
auto MarketDataConsumer::StartSnapshotSync() -> void {
    snapshot_queued_msgs_.clear();
    incremental_queued_msgs_.clear();

    ASSERT(snapshot_mcast_socket_.Init(SNAPSHOT_IP, IFACE, SNAPSHOT_PORT, /*is_listening*/ true) >= 0,
           "Unable to create snapshot mcast socket. error:" + std::string(std::strerror(errno)));
    ASSERT(snapshot_mcast_socket_.Join(SNAPSHOT_IP),  // IGMP multicast subscription.
           "Join failed on:" + std::to_string(snapshot_mcast_socket_.socket_fd_) +
               " error:" + std::string(std::strerror(errno)));
}

// Check if a recovery / synchronization is possible from the queued up market data updates from the snapshot and
// incremental market data streams.
auto MarketDataConsumer::CheckSnapshotSync() -> void {
    if (snapshot_queued_msgs_.empty()) {
        return;
    }

    const auto &first_snapshot_msg = snapshot_queued_msgs_.begin()->second;
    if (first_snapshot_msg.type_ != exchange::MarketUpdateType::SNAPSHOT_START) {
        logger_.Log("%:% %() % Returning because have not seen a SNAPSHOT_START yet.\n", __FILE__, __LINE__,
                    __FUNCTION__, common::GetCurrentTimeStr(&time_str_));
        snapshot_queued_msgs_.clear();
        return;
    }

    std::vector<exchange::MEMarketUpdate> final_events;

    auto have_complete_snapshot = true;
    size_t next_snapshot_seq = 0;
    for (auto &snapshot_itr : snapshot_queued_msgs_) {
        logger_.Log("%:% %() % % => %\n", __FILE__, __LINE__, __FUNCTION__, common::GetCurrentTimeStr(&time_str_),
                    snapshot_itr.first, snapshot_itr.second.ToString());
        if (snapshot_itr.first != next_snapshot_seq) {
            have_complete_snapshot = false;
            logger_.Log("%:% %() % Detected gap in snapshot stream expected:% found:% %.\n", __FILE__, __LINE__,
                        __FUNCTION__, common::GetCurrentTimeStr(&time_str_), next_snapshot_seq, snapshot_itr.first,
                        snapshot_itr.second.ToString());
            break;
        }

        if (snapshot_itr.second.type_ != exchange::MarketUpdateType::SNAPSHOT_START &&
            snapshot_itr.second.type_ != exchange::MarketUpdateType::SNAPSHOT_END) {
            final_events.push_back(snapshot_itr.second);
        }

        ++next_snapshot_seq;
    }

    if (!have_complete_snapshot) {
        logger_.Log("%:% %() % Returning because found gaps in snapshot stream.\n", __FILE__, __LINE__, __FUNCTION__,
                    common::GetCurrentTimeStr(&time_str_));
        snapshot_queued_msgs_.clear();
        return;
    }

    const auto &last_snapshot_msg = snapshot_queued_msgs_.rbegin()->second;
    if (last_snapshot_msg.type_ != exchange::MarketUpdateType::SNAPSHOT_END) {
        logger_.Log("%:% %() % Returning because have not seen a SNAPSHOT_END yet.\n", __FILE__, __LINE__, __FUNCTION__,
                    common::GetCurrentTimeStr(&time_str_));
        return;
    }

    auto have_complete_incremental = true;
    size_t num_incrementals = 0;
    next_exp_inc_seq_num_ = last_snapshot_msg.order_id_ + 1;
    for (auto &incremental_queued_msg : incremental_queued_msgs_) {
        logger_.Log("%:% %() % Checking next_exp:% vs. seq:% %.\n", __FILE__, __LINE__, __FUNCTION__,
                    common::GetCurrentTimeStr(&time_str_), next_exp_inc_seq_num_, incremental_queued_msg.first,
                    incremental_queued_msg.second.ToString());

        if (incremental_queued_msg.first < next_exp_inc_seq_num_) {
            continue;
        }

        if (incremental_queued_msg.first != next_exp_inc_seq_num_) {
            logger_.Log("%:% %() % Detected gap in incremental stream expected:% found:% %.\n", __FILE__, __LINE__,
                        __FUNCTION__, common::GetCurrentTimeStr(&time_str_), next_exp_inc_seq_num_,
                        incremental_queued_msg.first, incremental_queued_msg.second.ToString());
            have_complete_incremental = false;
            break;
        }

        logger_.Log("%:% %() % % => %\n", __FILE__, __LINE__, __FUNCTION__, common::GetCurrentTimeStr(&time_str_),
                    incremental_queued_msg.first, incremental_queued_msg.second.ToString());

        if (incremental_queued_msg.second.type_ != exchange::MarketUpdateType::SNAPSHOT_START &&
            incremental_queued_msg.second.type_ != exchange::MarketUpdateType::SNAPSHOT_END) {
            final_events.push_back(incremental_queued_msg.second);
        }

        ++next_exp_inc_seq_num_;
        ++num_incrementals;
    }

    if (!have_complete_incremental) {
        logger_.Log("%:% %() % Returning because have gaps in queued incrementals.\n", __FILE__, __LINE__, __FUNCTION__,
                    common::GetCurrentTimeStr(&time_str_));
        snapshot_queued_msgs_.clear();
        return;
    }

    for (const auto &itr : final_events) {
        auto next_write = incoming_md_updates_->GetNextToWriteTo();
        *next_write = itr;
        incoming_md_updates_->UpdateWriteIndex();
    }

    logger_.Log("%:% %() % Recovered % snapshot and % incremental orders.\n", __FILE__, __LINE__, __FUNCTION__,
                common::GetCurrentTimeStr(&time_str_), snapshot_queued_msgs_.size() - 2, num_incrementals);

    snapshot_queued_msgs_.clear();
    incremental_queued_msgs_.clear();
    in_recovery_ = false;

    snapshot_mcast_socket_.Leave(SNAPSHOT_IP, SNAPSHOT_PORT);
    ;
}

// Queue up a message in the *_queued_msgs_ containers, first parameter specifies if this update came from the snapshot
// or the incremental streams.
auto MarketDataConsumer::QueueMessage(bool is_snapshot, const exchange::MDPMarketUpdate *request) {
    if (is_snapshot) {
        if (snapshot_queued_msgs_.contains(request->seq_num_)) {
            logger_.Log("%:% %() % Packet drops on snapshot socket. Received for a 2nd time:%\n", __FILE__, __LINE__,
                        __FUNCTION__, common::GetCurrentTimeStr(&time_str_), request->ToString());
            snapshot_queued_msgs_.clear();
        }
        snapshot_queued_msgs_[request->seq_num_] = request->me_market_update_;
    } else {
        incremental_queued_msgs_[request->seq_num_] = request->me_market_update_;
    }

    logger_.Log("%:% %() % size snapshot:% incremental:% % => %\n", __FILE__, __LINE__, __FUNCTION__,
                common::GetCurrentTimeStr(&time_str_), snapshot_queued_msgs_.size(), incremental_queued_msgs_.size(),
                request->seq_num_, request->ToString());

    CheckSnapshotSync();
}

// Process a market data update, the consumer needs to use the socket parameter to figure out whether this came from the
// snapshot or the incremental stream.
auto MarketDataConsumer::RecvCallback(common::McastSocket *socket) noexcept -> void {
    const auto is_snapshot = (socket->socket_fd_ == snapshot_mcast_socket_.socket_fd_);
    if (is_snapshot && !in_recovery_) [[unlikely]] {  // market update was read from the snapshot market data stream and
                                                      // we are not in recovery, so we dont need it and discard it.
        socket->next_rcv_valid_index_ = 0;

        logger_.Log("%:% %() % WARN Not expecting snapshot messages.\n", __FILE__, __LINE__, __FUNCTION__,
                    common::GetCurrentTimeStr(&time_str_));

        return;
    }

    if (socket->next_rcv_valid_index_ >= sizeof(exchange::MDPMarketUpdate)) {
        size_t i = 0;
        for (; i + sizeof(exchange::MDPMarketUpdate) <= socket->next_rcv_valid_index_;
             i += sizeof(exchange::MDPMarketUpdate)) {
            auto request = reinterpret_cast<const exchange::MDPMarketUpdate *>(socket->inbound_data_.data() + i);
            logger_.Log("%:% %() % Received % socket len:% %\n", __FILE__, __LINE__, __FUNCTION__,
                        common::GetCurrentTimeStr(&time_str_), (is_snapshot ? "snapshot" : "incremental"),
                        sizeof(exchange::MDPMarketUpdate), request->ToString());

            const bool already_in_recovery = in_recovery_;
            in_recovery_ = (already_in_recovery || request->seq_num_ != next_exp_inc_seq_num_);

            if (in_recovery_) [[unlikely]] {
                if (!already_in_recovery)
                    [[unlikely]] {  // if we just entered recovery, start the snapshot synchonization process by
                                    // subscribing to the snapshot multicast stream.
                    logger_.Log("%:% %() % Packet drops on % socket. SeqNum expected:% received:%\n", __FILE__,
                                __LINE__, __FUNCTION__, common::GetCurrentTimeStr(&time_str_),
                                (is_snapshot ? "snapshot" : "incremental"), next_exp_inc_seq_num_, request->seq_num_);
                    StartSnapshotSync();
                }

                QueueMessage(is_snapshot, request);  // queue up the market data update message and check if snapshot
                                                     // recovery / synchronization can be completed successfully.
            } else if (!is_snapshot) {  // not in recovery and received a packet in the correct order and without gaps,
                                        // process it.
                logger_.Log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, common::GetCurrentTimeStr(&time_str_),
                            request->ToString());

                ++next_exp_inc_seq_num_;

                auto next_write = incoming_md_updates_->GetNextToWriteTo();
                *next_write = request->me_market_update_;
                incoming_md_updates_->UpdateWriteIndex();
            }
        }

        // Shift down leftover bytes to the start of inbound_data_.
        memcpy(socket->inbound_data_.data(), socket->inbound_data_.data() + i, socket->next_rcv_valid_index_ - i);
        socket->next_rcv_valid_index_ -= i;
    }
}

}  // namespace trading
