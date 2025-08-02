#include "network/tcp_socket.hpp"

namespace common {

// Create TCPSocket with provided attributes to either listen-on / connect-to.
auto TCPSocket::Connect(const std::string &ip, const std::string &iface, int port, bool is_listening) -> int {
    // Note that needs_so_timestamp=true for FIFOSequencer.
    const SocketCfg socket_cfg{.ip_ = ip,
                               .iface_ = iface,
                               .port_ = port,
                               .is_udp_ = false,
                               .is_listening_ = is_listening,
                               .needs_so_timestamp_ = true};

    socket_fd_ = CreateSocket(logger_, socket_cfg);
    socket_attrib_.sin_addr.s_addr = INADDR_ANY;
    socket_attrib_.sin_port = htons(port);
    socket_attrib_.sin_family = AF_INET;

    return socket_fd_;
}

// Called to publish outgoing data from the buffers as well as check for and callback if data is available in the read
// buffers.
auto TCPSocket::SendAndRecv() noexcept -> bool {
    char ctrl[CMSG_SPACE(sizeof(struct timeval))];
    auto cmsg = reinterpret_cast<struct cmsghdr *>(&ctrl);

    iovec iov{.iov_base = inbound_data_.data() + next_rcv_valid_index_,
              .iov_len = TCP_BUFFER_SIZE - next_rcv_valid_index_};
    msghdr msg{.msg_name = &socket_attrib_,
               .msg_namelen = sizeof(socket_attrib_),
               .msg_iov = &iov,
               .msg_iovlen = 1,
               .msg_control = ctrl,
               .msg_controllen = sizeof(ctrl),
               .msg_flags = 0};

    // Non-blocking call to read available data.
    const auto read_size = recvmsg(socket_fd_, &msg, MSG_DONTWAIT);
    if (read_size > 0) {
        next_rcv_valid_index_ += read_size;

        Nanos kernel_time = 0;
        timeval time_kernel;
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_TIMESTAMP &&
            cmsg->cmsg_len == CMSG_LEN(sizeof(time_kernel))) {
            memcpy(&time_kernel, CMSG_DATA(cmsg), sizeof(time_kernel));
            kernel_time = time_kernel.tv_sec * NANOS_TO_SECS +
                          time_kernel.tv_usec * NANOS_TO_MICROS;  // convert timestamp to nanoseconds.
        }

        const auto user_time = GetCurrentNanos();

        logger_.Log("%:% %() % read socket:% len:% utime:% ktime:% diff:%\n", __FILE__, __LINE__, __FUNCTION__,
                    GetCurrentTimeStr(&time_str_), socket_fd_, next_rcv_valid_index_, user_time, kernel_time,
                    (user_time - kernel_time));
        recv_callback_(this, kernel_time);
    }

    if (next_send_valid_index_ > 0) {
        // Non-blocking call to send data.
        const auto n = ::send(socket_fd_, outbound_data_.data(), next_send_valid_index_, MSG_DONTWAIT | MSG_NOSIGNAL);
        logger_.Log("%:% %() % send socket:% len:%\n", __FILE__, __LINE__, __FUNCTION__, GetCurrentTimeStr(&time_str_),
                    socket_fd_, n);
    }
    next_send_valid_index_ = 0;

    return (read_size > 0);
}

// Write outgoing data to the send buffers.
void TCPSocket::Send(const void *data, size_t len) noexcept {
    memcpy(outbound_data_.data() + next_send_valid_index_, data, len);
    next_send_valid_index_ += len;
}

}  // namespace common
