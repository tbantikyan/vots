/*
 * socket_utils.hpp
 * Low level network code for creating an efficient socket.
 */

#pragma once

#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

#include "integrity/integrity.hpp"
#include "logging/logger.hpp"

namespace common {

struct SocketCfg {
    std::string ip_;
    std::string iface_;
    int port_ = -1;
    bool is_udp_ = false;
    bool is_listening_ = false;
    bool needs_so_timestamp_ = false;

    auto ToString() const {
        std::stringstream ss;
        ss << "SocketCfg[ip:" << ip_ << " iface:" << iface_ << " port:" << port_ << " is_udp:" << is_udp_
           << " is_listening:" << is_listening_ << " needs_SO_timestamp:" << needs_so_timestamp_ << "]";

        return ss.str();
    }
};

// Represents the maximum number of pending / unaccepted TCP connections.
constexpr int MAX_TCP_SERVER_BACKLOG = 1024;

// Convert interface name to IP address for lower level routines (e.g. "eth0" to ip "192.168.10.104").
inline auto GetIfaceIp(const std::string &iface) -> std::string {
    char buf[NI_MAXHOST] = {'\0'};
    ifaddrs *ifaddr = nullptr;

    if (getifaddrs(&ifaddr) != -1) {  // getifaddrs returns info for all interfaces as a linked list.
        for (ifaddrs *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if ((ifa->ifa_addr != nullptr) && ifa->ifa_addr->sa_family == AF_INET && iface == ifa->ifa_name) {
                getnameinfo(ifa->ifa_addr, sizeof(sockaddr_in), buf, sizeof(buf), nullptr, 0, NI_NUMERICHOST);
                break;
            }
        }
        freeifaddrs(ifaddr);
    }

    return buf;
}

/*
 * Sockets will not block on read, but instead return immediately if data is not available. Blocking is particularly bad
 * for low latency because it requires context switching + interrupt handling.
 *
 * Returns true if socket already was or successfully is set to non-blocking.
 */
inline auto SetNonBlocking(int fd) -> bool {
    const auto flags = fcntl(fd, F_GETFL, 0);
    if ((flags & O_NONBLOCK) != 0) {
        return true;  // Socket file descriptor indicates socket is already non-blocking.
    }
    return (fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1);
}

// Disable Nagle's algorithm and associated delays.
inline auto DisableNagle(int fd) -> bool {
    int one = 1;
    return (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<void *>(&one), sizeof(one)) != -1);
}

// Allow software receive timestamps on incoming packets.
inline auto SetSoTimestamp(int fd) -> bool {
    int one = 1;
    return (setsockopt(fd, SOL_SOCKET, SO_TIMESTAMP, reinterpret_cast<void *>(&one), sizeof(one)) != -1);
}

// Add / Join membership / subscription to the multicast stream specified and on the interface specified.
inline auto Join(int fd, const std::string &ip) -> bool {
    const ip_mreq mreq{.imr_multiaddr = {inet_addr(ip.c_str())}, .imr_interface = {htonl(INADDR_ANY)}};
    return (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) != -1);
}

/*
 * Create a TCP / UDP socket to either connect to or listen for data on or listen for connections on the specified
 * interface and IP:port information. Configures it to be non-blocking with Nagle's disabled (for TCP). Does not utilize
 * kernel bypass.
 *
 * Returns -1 to signify failure.
 */
[[nodiscard]] inline auto CreateSocket(Logger &logger, const SocketCfg &socket_cfg) -> int {
    std::string time_str;

    const auto ip = socket_cfg.ip_.empty() ? GetIfaceIp(socket_cfg.iface_) : socket_cfg.ip_;
    logger.Log("%:% %() % cfg:%\n", __FILE__, __LINE__, __FUNCTION__, common::GetCurrentTimeStr(&time_str),
               socket_cfg.ToString());

    const int input_flags = (socket_cfg.is_listening_ ? AI_PASSIVE : 0) | (AI_NUMERICHOST | AI_NUMERICSERV);
    const addrinfo hints{.ai_flags = input_flags,
                         .ai_family = AF_INET,
                         .ai_socktype = socket_cfg.is_udp_ ? SOCK_DGRAM : SOCK_STREAM,
                         .ai_protocol = socket_cfg.is_udp_ ? IPPROTO_UDP : IPPROTO_TCP,
                         .ai_addrlen = 0,
                         .ai_addr = nullptr,
                         .ai_canonname = nullptr,
                         .ai_next = nullptr};

    // Fetch linked-list of socket addresses.
    addrinfo *result = nullptr;
    const auto rc = getaddrinfo(ip.c_str(), std::to_string(socket_cfg.port_).c_str(), &hints, &result);
    ASSERT(rc == 0, "getaddrinfo() failed. error:" + std::string(gai_strerror(rc)) + "errno:" + strerror(errno));

    int socket_fd = -1;
    int one = 1;
    for (addrinfo *rp = result; rp != nullptr; rp = rp->ai_next) {
        ASSERT((socket_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) != -1,
               "socket() failed. errno:" + std::string(strerror(errno)));

        ASSERT(SetNonBlocking(socket_fd), "setNonBlocking() failed. errno:" + std::string(strerror(errno)));

        if (!socket_cfg.is_udp_) {  // disable Nagle for TCP sockets.
            ASSERT(DisableNagle(socket_fd), "disableNagle() failed. errno:" + std::string(strerror(errno)));
        }

        if (!socket_cfg.is_listening_) {  // establish connection to specified address.
            ASSERT(connect(socket_fd, rp->ai_addr, rp->ai_addrlen) != 1,
                   "connect() failed. errno:" + std::string(strerror(errno)));
        }

        if (socket_cfg.is_listening_) {  // allow re-using the address in the call to bind()
            ASSERT(
                setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&one), sizeof(one)) == 0,
                "setsockopt() SO_REUSEADDR failed. errno:" + std::string(strerror(errno)));
        }

        if (socket_cfg.is_listening_) {
            // bind to the specified port number.
            const sockaddr_in addr{.sin_family = AF_INET,
                                   .sin_port = htons(socket_cfg.port_),
                                   .sin_addr = {htonl(INADDR_ANY)},
                                   .sin_zero = {}};
            ASSERT(bind(socket_fd, socket_cfg.is_udp_ ? reinterpret_cast<const struct sockaddr *>(&addr) : rp->ai_addr,
                        sizeof(addr)) == 0,
                   "bind() failed. errno:%" + std::string(strerror(errno)));
        }

        if (!socket_cfg.is_udp_ && socket_cfg.is_listening_) {  // listen for incoming TCP connections.
            ASSERT(listen(socket_fd, MAX_TCP_SERVER_BACKLOG) == 0,
                   "listen() failed. errno:" + std::string(strerror(errno)));
        }

        if (socket_cfg.needs_so_timestamp_) {  // enable software receive timestamps.
            ASSERT(SetSoTimestamp(socket_fd), "setSOTimestamp() failed. errno:" + std::string(strerror(errno)));
        }
    }

    return socket_fd;
}

}  // namespace common
