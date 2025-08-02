#include "market_data/market_data_publisher.hpp"

namespace exchange {

MarketDataPublisher::MarketDataPublisher(MEMarketUpdateLFQueue *market_updates, const std::string &iface,
                                         const std::string &snapshot_ip, int snapshot_port,
                                         const std::string &incremental_ip, int incremental_port)
    : outgoing_md_updates_(market_updates),
      snapshot_md_updates_(common::ME_MAX_MARKET_UPDATES),
      run_(false), // NOLINT
      logger_("exchange_market_data_publisher.log"),
      incremental_socket_(logger_) {
    ASSERT(incremental_socket_.Init(incremental_ip, iface, incremental_port, /*is_listening*/ false) >= 0,
           "Unable to create incremental mcast socket. error:" + std::string(std::strerror(errno)));
    snapshot_synthesizer_ = new SnapshotSynthesizer(&snapshot_md_updates_, iface, snapshot_ip, snapshot_port);
}

auto MarketDataPublisher::Run() noexcept -> void {
    logger_.Log("%:% %() %\n", __FILE__, __LINE__, __FUNCTION__, common::GetCurrentTimeStr(&time_str_));
    while (run_) {
        for (auto market_update = outgoing_md_updates_->GetNextToRead();
             (outgoing_md_updates_->Size() != 0) && (market_update != nullptr);
             market_update = outgoing_md_updates_->GetNextToRead()) {
            logger_.Log("%:% %() % Sending seq:% %\n", __FILE__, __LINE__, __FUNCTION__,
                        common::GetCurrentTimeStr(&time_str_), next_inc_seq_num_, market_update->ToString().c_str());

            // Sends an MDPMarketUpdate as its components (a sequence number followed by an MEMarketUpdate).
            incremental_socket_.Send(&next_inc_seq_num_, sizeof(next_inc_seq_num_));
            incremental_socket_.Send(market_update, sizeof(MEMarketUpdate));
            outgoing_md_updates_->UpdateReadIndex();

            auto next_write = snapshot_md_updates_.GetNextToWriteTo();
            next_write->seq_num_ = next_inc_seq_num_;
            next_write->me_market_update_ = *market_update;
            snapshot_md_updates_.UpdateWriteIndex();

            ++next_inc_seq_num_;
        }

        incremental_socket_.SendAndRecv();
    }
}

}  // namespace exchange
