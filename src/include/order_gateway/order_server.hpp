/*
 * order_server.hpp
 * Defines the order gateway server that accepts new connections to the exchange from market participants. The server
 * also handles incoming client requests, sequencing them in FIFO order for fairness and passing them on to the matching
 * engine.
 */

#pragma once

#include <functional>

#include "client_request.hpp"
#include "client_response.hpp"
#include "common/integrity.hpp"
#include "common/perf_utils.hpp"
#include "fifo_sequencer.hpp"
#include "network/tcp_server.hpp"
#include "runtime/threads.hpp"

namespace exchange {

class OrderServer {
   public:
    OrderServer(ClientRequestLFQueue *client_requests, ClientResponseLFQueue *client_responses,
                const std::string &iface, int port);

    ~OrderServer();

    // Start and stop the order server main thread.
    void Start();

    void Stop();

    // Main run loop for this thread - accepts new client connections, receives client requests from them and sends
    // client responses to them.
    auto Run() noexcept {
        logger_.Log("%:% %() %\n", __FILE__, __LINE__, __FUNCTION__, common::GetCurrentTimeStr(&time_str_));
        while (run_) {
            tcp_server_.Poll();

            tcp_server_.SendAndRecv();

            for (auto client_response = outgoing_responses_->GetNextToRead();
                 (outgoing_responses_->Size() != 0) && (client_response != nullptr);
                 client_response = outgoing_responses_->GetNextToRead()) {
                TTT_MEASURE(t5t_order_server_lf_queue_read, logger_);

                auto &next_outgoing_seq_num = cid_next_outgoing_seq_num_[client_response->client_id_];
                logger_.Log("%:% %() % Processing cid:% seq:% %\n", __FILE__, __LINE__, __FUNCTION__,
                            common::GetCurrentTimeStr(&time_str_), client_response->client_id_, next_outgoing_seq_num,
                            client_response->ToString());

                ASSERT(cid_tcp_socket_[client_response->client_id_] != nullptr,
                       "Dont have a TCPSocket for ClientId:" + std::to_string(client_response->client_id_));
                START_MEASURE(exchange_tcp_socket_send);
                // Sends an OMClientResponse as its components (a sequence number followed by an MEClientResponse).
                cid_tcp_socket_[client_response->client_id_]->Send(&next_outgoing_seq_num,
                                                                   sizeof(next_outgoing_seq_num));
                cid_tcp_socket_[client_response->client_id_]->Send(client_response, sizeof(MEClientResponse));
                END_MEASURE(exchange_tcp_socket_send, logger_);

                outgoing_responses_->UpdateReadIndex();
                TTT_MEASURE(t6t_order_server_tcp_write, logger_);

                ++next_outgoing_seq_num;
            }
        }
    }

    // Read client request from the TCP receive buffer, check for sequence gaps and forward it to the FIFO sequencer.
    auto RecvCallback(common::TCPSocket *socket, common::Nanos rx_time) noexcept {
        TTT_MEASURE(t1_order_server_tcp_read, logger_);

        logger_.Log("%:% %() % Received socket:% len:% rx:%\n", __FILE__, __LINE__, __FUNCTION__,
                    common::GetCurrentTimeStr(&time_str_), socket->socket_fd_, socket->next_rcv_valid_index_, rx_time);

        if (socket->next_rcv_valid_index_ >= sizeof(OMClientRequest)) {
            size_t i = 0;
            for (; i + sizeof(OMClientRequest) <= socket->next_rcv_valid_index_; i += sizeof(OMClientRequest)) {
                auto request = reinterpret_cast<const OMClientRequest *>(socket->inbound_data_.data() + i);
                logger_.Log("%:% %() % Received %\n", __FILE__, __LINE__, __FUNCTION__,
                            common::GetCurrentTimeStr(&time_str_), request->ToString());

                if (cid_tcp_socket_[request->me_client_request_.client_id_] == nullptr) [[unlikely]] {
                    cid_tcp_socket_[request->me_client_request_.client_id_] = socket;
                }

                if (cid_tcp_socket_[request->me_client_request_.client_id_] !=
                    socket) {  // TODO(tbantikyan) - change this to send a reject back to the client.
                    logger_.Log("%:% %() % Received ClientRequest from ClientId:% on different socket:% expected:%\n",
                                __FILE__, __LINE__, __FUNCTION__, common::GetCurrentTimeStr(&time_str_),
                                request->me_client_request_.client_id_, socket->socket_fd_,
                                cid_tcp_socket_[request->me_client_request_.client_id_]->socket_fd_);
                    continue;
                }

                auto &next_exp_seq_num = cid_next_exp_seq_num_[request->me_client_request_.client_id_];
                if (request->seq_num_ !=
                    next_exp_seq_num) {  // TODO(tbantikyan) - change this to send a reject back to the client.
                    logger_.Log("%:% %() % Incorrect sequence number. ClientId:% SeqNum expected:% received:%\n",
                                __FILE__, __LINE__, __FUNCTION__, common::GetCurrentTimeStr(&time_str_),
                                request->me_client_request_.client_id_, next_exp_seq_num, request->seq_num_);
                    continue;
                }

                ++next_exp_seq_num;

                START_MEASURE(exchange_fifo_sequencer_add_client_request);
                fifo_sequencer_.AddClientRequest(rx_time, request->me_client_request_);
                END_MEASURE(exchange_fifo_sequencer_add_client_request, logger_);
            }

            // Shift down leftover bytes to the start of inbound_data_.
            memcpy(socket->inbound_data_.data(), socket->inbound_data_.data() + i, socket->next_rcv_valid_index_ - i);
            socket->next_rcv_valid_index_ -= i;
        }
    }

    // End of reading incoming messages across all the TCP connections, sequence and publish the client requests to the
    // matching engine.
    auto RecvFinishedCallback() noexcept {
        START_MEASURE(exchange_fifo_sequencer_sequence_and_publish);
        fifo_sequencer_.SequenceAndPublish();
        END_MEASURE(exchange_fifo_sequencer_sequence_and_publish, logger_);
    }

    // Deleted default, copy & move constructors and assignment-operators.
    OrderServer() = delete;

    OrderServer(const OrderServer &) = delete;

    OrderServer(const OrderServer &&) = delete;

    auto operator=(const OrderServer &) -> OrderServer & = delete;

    auto operator=(const OrderServer &&) -> OrderServer & = delete;

   private:
    const std::string IFACE;
    const int PORT = 0;

    // Lock free queue of outgoing client responses to be sent out to connected clients.
    ClientResponseLFQueue *outgoing_responses_ = nullptr;

    volatile bool run_ = false;

    std::string time_str_;
    common::Logger logger_;

    // Hash map from ClientId -> the next sequence number to be sent on outgoing client responses.
    std::array<size_t, common::ME_MAX_NUM_CLIENTS> cid_next_outgoing_seq_num_;

    // Hash map from ClientId -> the next sequence number expected on incoming client requests.
    std::array<size_t, common::ME_MAX_NUM_CLIENTS> cid_next_exp_seq_num_;

    // Hash map from ClientId -> TCP socket / client connection.
    std::array<common::TCPSocket *, common::ME_MAX_NUM_CLIENTS> cid_tcp_socket_;

    // TCP server instance listening for new client connections.
    common::TCPServer tcp_server_;

    // FIFO sequencer responsible for making sure incoming client requests are processed in the order in which they
    // were received.
    FIFOSequencer fifo_sequencer_;
};

}  // namespace exchange
