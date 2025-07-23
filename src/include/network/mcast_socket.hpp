#pragma once

#include <functional>

#include "logging/logger.hpp"
#include "socket_utils.hpp"

namespace common {

// Size of send and receive buffers in bytes.
constexpr size_t MCAST_BUFFER_SIZE = 64 * 1024 * 1024;

struct McastSocket {
    explicit McastSocket(Logger &logger) : logger_(logger) {
        outbound_data_.resize(MCAST_BUFFER_SIZE);
        inbound_data_.resize(MCAST_BUFFER_SIZE);
    }

    // Initialize multicast socket to read from or publish to a stream.
    // Does not join the multicast stream yet.
    auto Init(const std::string &ip, const std::string &iface, int port, bool is_listening) -> int;

    // Add / Join membership / subscription to a multicast stream.
    auto Join(const std::string &ip) -> bool;

    // Remove / Leave membership / subscription to a multicast stream.
    auto Leave(const std::string &ip, int port) -> void;

    // Publish outgoing data and read incoming data.
    auto SendAndRecv() noexcept -> bool;

    // Copy data to send buffers - does not send them out yet.
    auto Send(const void *data, size_t len) noexcept -> void;

    int socket_fd_ = -1;

    // Send and receive buffers, typically only one or the other is needed, not both.
    std::vector<char> outbound_data_;
    size_t next_send_valid_index_ = 0;
    std::vector<char> inbound_data_;
    size_t next_rcv_valid_index_ = 0;

    // Function wrapper for the method to call when data is read.
    std::function<void(McastSocket *s)> recv_callback_ = nullptr;

    std::string time_str_;
    Logger &logger_;
};

}  // namespace common
