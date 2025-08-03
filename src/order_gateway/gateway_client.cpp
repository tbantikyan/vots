#include "order_gateway/gateway_client.hpp"

namespace trading {

GatewayClient::GatewayClient(common::ClientId client_id, exchange::ClientRequestLFQueue *client_requests,
                             exchange::ClientResponseLFQueue *client_responses,
                             std::string ip,            // NOLINT
                             const std::string &iface,  // NOLINT
                             int port)
    : CLIENT_ID(client_id),
      ip_(ip),  // NOLINT
      IFACE(iface),
      PORT(port),
      outgoing_requests_(client_requests),
      incoming_responses_(client_responses),
      logger_("trading_order_gateway_" + std::to_string(client_id) + ".log"),
      tcp_socket_(logger_) {
    tcp_socket_.recv_callback_ = [this](auto socket, auto rx_time) { RecvCallback(socket, rx_time); };
}

// Main thread loop - sends out client requests to the exchange and reads and dispatches incoming client responses.
void GatewayClient::Run() noexcept {
    logger_.Log("%:% %() %\n", __FILE__, __LINE__, __FUNCTION__, common::GetCurrentTimeStr(&time_str_));
    while (run_) {
        tcp_socket_.SendAndRecv();

        for (auto client_request = outgoing_requests_->GetNextToRead(); client_request != nullptr;
             client_request = outgoing_requests_->GetNextToRead()) {
            logger_.Log("%:% %() % Sending cid:% seq:% %\n", __FILE__, __LINE__, __FUNCTION__,
                        common::GetCurrentTimeStr(&time_str_), CLIENT_ID, next_outgoing_seq_num_,
                        client_request->ToString());
            tcp_socket_.Send(&next_outgoing_seq_num_, sizeof(next_outgoing_seq_num_));
            tcp_socket_.Send(client_request, sizeof(exchange::MEClientRequest));
            outgoing_requests_->UpdateReadIndex();

            next_outgoing_seq_num_++;
        }
    }
}

// Callback when an incoming client response is read, we perform some checks and forward it to the lock free queue
// connected to the trade engine.
auto GatewayClient::RecvCallback(common::TCPSocket *socket, common::Nanos rx_time) noexcept -> void {
    logger_.Log("%:% %() % Received socket:% len:% %\n", __FILE__, __LINE__, __FUNCTION__,
                common::GetCurrentTimeStr(&time_str_), socket->socket_fd_, socket->next_rcv_valid_index_, rx_time);

    if (socket->next_rcv_valid_index_ >= sizeof(exchange::OMClientResponse)) {
        size_t i = 0;
        for (; i + sizeof(exchange::OMClientResponse) <= socket->next_rcv_valid_index_;
             i += sizeof(exchange::OMClientResponse)) {
            auto response = reinterpret_cast<const exchange::OMClientResponse *>(socket->inbound_data_.data() + i);
            logger_.Log("%:% %() % Received %\n", __FILE__, __LINE__, __FUNCTION__,
                        common::GetCurrentTimeStr(&time_str_), response->ToString());

            if (response->me_client_response_.client_id_ !=
                CLIENT_ID) {  // this should never happen unless there is a bug at the exchange.
                logger_.Log("%:% %() % ERROR Incorrect client id. ClientId expected:% received:%.\n", __FILE__,
                            __LINE__, __FUNCTION__, common::GetCurrentTimeStr(&time_str_), CLIENT_ID,
                            response->me_client_response_.client_id_);
                continue;
            }
            if (response->seq_num_ != next_exp_seq_num_) {  // this should never happen since we use a reliable TCP
                                                            // protocol, unless there is a bug at the exchange.
                logger_.Log("%:% %() % ERROR Incorrect sequence number. ClientId:%. SeqNum expected:% received:%.\n",
                            __FILE__, __LINE__, __FUNCTION__, common::GetCurrentTimeStr(&time_str_), CLIENT_ID,
                            next_exp_seq_num_, response->seq_num_);
                continue;
            }

            ++next_exp_seq_num_;

            auto next_write = incoming_responses_->GetNextToWriteTo();
            *next_write = response->me_client_response_;
            incoming_responses_->UpdateWriteIndex();
        }
        memcpy(socket->inbound_data_.data(), socket->inbound_data_.data() + i, socket->next_rcv_valid_index_ - i);
        socket->next_rcv_valid_index_ -= i;
    }
}

}  // namespace trading
