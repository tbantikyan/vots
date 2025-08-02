#include "market_data/snapshot_synthesizer.hpp"

namespace exchange {

SnapshotSynthesizer::SnapshotSynthesizer(MDPMarketUpdateLFQueue *market_updates, const std::string &iface,
                                         const std::string &snapshot_ip, int snapshot_port)
    : snapshot_md_updates_(market_updates),
      logger_("exchange_snapshot_synthesizer.log"),
      snapshot_socket_(logger_),
      order_pool_(common::ME_MAX_ORDER_IDS) {
    ASSERT(snapshot_socket_.Init(snapshot_ip, iface, snapshot_port, /*is_listening*/ false) >= 0,
           "Unable to create snapshot mcast socket. error:" + std::string(std::strerror(errno)));
    for (auto &orders : ticker_orders_) {
        orders.fill(nullptr);
    }
}

SnapshotSynthesizer::~SnapshotSynthesizer() { Stop(); }

void SnapshotSynthesizer::Start() {
    run_ = true;
    ASSERT(common::CreateAndStartThread(-1, "exchange/SnapshotSynthesizer", [this]() { Run(); }) != nullptr,
           "Failed to start SnapshotSynthesizer thread.");
}

void SnapshotSynthesizer::Stop() { run_ = false; }

auto SnapshotSynthesizer::AddToSnapshot(const MDPMarketUpdate *market_update) {
    const auto &me_market_update = market_update->me_market_update_;
    auto *orders = &ticker_orders_.at(me_market_update.ticker_id_);
    switch (me_market_update.type_) {
        case MarketUpdateType::ADD: {
            auto order = orders->at(me_market_update.order_id_);
            ASSERT(order == nullptr, "Received:" + me_market_update.ToString() +
                                         " but order already exists:" + ((order != nullptr) ? order->ToString() : ""));
            orders->at(me_market_update.order_id_) = order_pool_.Allocate(me_market_update);
        } break;
        case MarketUpdateType::MODIFY: {
            auto order = orders->at(me_market_update.order_id_);
            ASSERT(order != nullptr, "Received:" + me_market_update.ToString() + " but order does not exist.");
            ASSERT(order->order_id_ == me_market_update.order_id_, "Expecting existing order to match new one.");
            ASSERT(order->side_ == me_market_update.side_, "Expecting existing order to match new one.");

            order->qty_ = me_market_update.qty_;
            order->price_ = me_market_update.price_;
        } break;
        case MarketUpdateType::CANCEL: {
            auto order = orders->at(me_market_update.order_id_);
            ASSERT(order != nullptr, "Received:" + me_market_update.ToString() + " but order does not exist.");
            ASSERT(order->order_id_ == me_market_update.order_id_, "Expecting existing order to match new one.");
            ASSERT(order->side_ == me_market_update.side_, "Expecting existing order to match new one.");

            order_pool_.Deallocate(order);
            orders->at(me_market_update.order_id_) = nullptr;
        } break;
        case MarketUpdateType::SNAPSHOT_START:
        case MarketUpdateType::CLEAR:
        case MarketUpdateType::SNAPSHOT_END:
        case MarketUpdateType::TRADE:
        case MarketUpdateType::INVALID:
            break;
    }

    ASSERT(market_update->seq_num_ == last_inc_seq_num_ + 1, "Expected incremental seq_nums to increase.");
    last_inc_seq_num_ = market_update->seq_num_;
}

auto SnapshotSynthesizer::PublishSnapshot() {
    size_t snapshot_size = 0;

    const MDPMarketUpdate start_market_update{
        .seq_num_ = snapshot_size++,
        .me_market_update_ = {.type_ = MarketUpdateType::SNAPSHOT_START, .order_id_ = last_inc_seq_num_}};
    logger_.Log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, common::GetCurrentTimeStr(&time_str_),
                start_market_update.ToString());
    snapshot_socket_.Send(&start_market_update, sizeof(MDPMarketUpdate));

    for (size_t ticker_id = 0; ticker_id < ticker_orders_.size(); ++ticker_id) {
        const auto &orders = ticker_orders_.at(ticker_id);

        MEMarketUpdate me_market_update;
        me_market_update.type_ = MarketUpdateType::CLEAR;
        me_market_update.ticker_id_ = ticker_id;

        const MDPMarketUpdate clear_market_update{.seq_num_ = snapshot_size++, .me_market_update_ = me_market_update};
        logger_.Log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, common::GetCurrentTimeStr(&time_str_),
                    clear_market_update.ToString());
        snapshot_socket_.Send(&clear_market_update, sizeof(MDPMarketUpdate));

        for (const auto order : orders) {
            if (order != nullptr) {
                const MDPMarketUpdate market_update{.seq_num_ = snapshot_size++, .me_market_update_ = *order};
                logger_.Log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, common::GetCurrentTimeStr(&time_str_),
                            market_update.ToString());
                snapshot_socket_.Send(&market_update, sizeof(MDPMarketUpdate));
                snapshot_socket_.SendAndRecv();
            }
        }
    }

    const MDPMarketUpdate end_market_update{
        .seq_num_ = snapshot_size++,
        .me_market_update_ = {.type_ = MarketUpdateType::SNAPSHOT_END, .order_id_ = last_inc_seq_num_}};
    logger_.Log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, common::GetCurrentTimeStr(&time_str_),
                end_market_update.ToString());
    snapshot_socket_.Send(&end_market_update, sizeof(MDPMarketUpdate));
    snapshot_socket_.SendAndRecv();

    logger_.Log("%:% %() % Published snapshot of % orders.\n", __FILE__, __LINE__, __FUNCTION__,
                common::GetCurrentTimeStr(&time_str_), snapshot_size - 1);
}

void SnapshotSynthesizer::Run() {
    logger_.Log("%:% %() %\n", __FILE__, __LINE__, __FUNCTION__, common::GetCurrentTimeStr(&time_str_));
    while (run_) {
        for (auto market_update = snapshot_md_updates_->GetNextToRead();
             (snapshot_md_updates_->Size() != 0) && (market_update != nullptr);
             market_update = snapshot_md_updates_->GetNextToRead()) {
            logger_.Log("%:% %() % Processing %\n", __FILE__, __LINE__, __FUNCTION__,
                        common::GetCurrentTimeStr(&time_str_), market_update->ToString().c_str());

            AddToSnapshot(market_update);

            snapshot_md_updates_->UpdateReadIndex();
        }

        if (common::GetCurrentNanos() - last_snapshot_time_ > 60 * common::NANOS_TO_SECS) {
            last_snapshot_time_ = common::GetCurrentNanos();
            PublishSnapshot();
        }
    }
}

}  // namespace exchange
