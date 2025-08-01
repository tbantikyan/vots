#include "order_server/order_server.hpp"

namespace exchange {
OrderServer::OrderServer(ClientRequestLFQueue *client_requests, ClientResponseLFQueue *client_responses,
                         const std::string &iface, int port)  // NOLINT
    : IFACE(iface),
      PORT(port),
      outgoing_responses_(client_responses),
      logger_("exchange_order_server.log"),
      tcp_server_(logger_),
      fifo_sequencer_(client_requests, &logger_) {
    cid_next_outgoing_seq_num_.fill(1);
    cid_next_exp_seq_num_.fill(1);
    cid_tcp_socket_.fill(nullptr);

    tcp_server_.recv_callback_ = [this](auto socket, auto rx_time) { RecvCallback(socket, rx_time); };
    tcp_server_.recv_finished_callback_ = [this]() { RecvFinishedCallback(); };
}

OrderServer::~OrderServer() {
    Stop();

    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(1s);
}

void OrderServer::Start() {
    run_ = true;
    tcp_server_.Listen(IFACE, PORT);

    ASSERT(common::CreateAndStartThread(-1, "Exchange/OrderServer", [this]() { Run(); }) != nullptr,
           "Failed to start OrderServer thread.");
}

void OrderServer::Stop() { run_ = false; }

}  // namespace exchange
