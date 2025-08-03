/*
 * gateway_client.hpp
 * Defines the component that the market participant uses to communicate with the trading exchange's order gateway.
 */

#pragma once

#include <functional>

#include "common/integrity.hpp"
#include "network/tcp_server.hpp"
#include "order_gateway/client_request.hpp"
#include "order_gateway/client_response.hpp"
#include "runtime/threads.hpp"

namespace trading {

class GatewayClient {
   public:
    GatewayClient(common::ClientId client_id, exchange::ClientRequestLFQueue *client_requests,
                  exchange::ClientResponseLFQueue *client_responses, std::string ip, const std::string &iface,
                  int port);

    ~GatewayClient() {
        Stop();

        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(5s);
    }

    void Start() {
        run_ = true;
        ASSERT(tcp_socket_.Connect(ip_, IFACE, PORT, false) >= 0,
               "Unable to connect to ip:" + ip_ + " port:" + std::to_string(PORT) + " on iface:" + IFACE +
                   " error:" + std::string(std::strerror(errno)));
        ASSERT(common::CreateAndStartThread(-1, "Trading/OrderGateway", [this]() { Run(); }) != nullptr,
               "Failed to start OrderGateway thread.");
    }

    void Stop() { run_ = false; }

    // Deleted default, copy & move constructors and assignment-operators.
    GatewayClient() = delete;

    GatewayClient(const GatewayClient &) = delete;

    GatewayClient(const GatewayClient &&) = delete;

    auto operator=(const GatewayClient &) -> GatewayClient & = delete;

    auto operator=(const GatewayClient &&) -> GatewayClient & = delete;

   private:
    const common::ClientId CLIENT_ID;

    std::string ip_;
    const std::string IFACE;
    const int PORT = 0;

    exchange::ClientRequestLFQueue *outgoing_requests_ = nullptr;
    exchange::ClientResponseLFQueue *incoming_responses_ = nullptr;

    volatile bool run_ = false;

    std::string time_str_;
    common::Logger logger_;

    size_t next_outgoing_seq_num_ = 1;
    size_t next_exp_seq_num_ = 1;
    common::TCPSocket tcp_socket_;

   private:
    void Run() noexcept;

    void RecvCallback(common::TCPSocket *socket, common::Nanos rx_time) noexcept;
};

}  // namespace trading
