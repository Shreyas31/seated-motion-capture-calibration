#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "udp_orientation_receiver.h"

#include <stdexcept>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>

namespace SeatedMoCap::Realtime {

class UdpOrientationReceiver::Impl {
  public:
    explicit Impl(const std::uint16_t port) {
        WSADATA winsockData{};
        if (WSAStartup(MAKEWORD(2, 2), &winsockData) != 0) {
            throw std::runtime_error("WSAStartup failed.");
        }
        winsockStarted_ = true;

        try {
            socket_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (socket_ == INVALID_SOCKET) {
                throw std::runtime_error("Could not create UDP receiver socket.");
            }

            sockaddr_in localAddress{};
            localAddress.sin_family = AF_INET;
            localAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            localAddress.sin_port = htons(port);

            if (bind(socket_, reinterpret_cast<const sockaddr*>(&localAddress),
                     sizeof(localAddress)) == SOCKET_ERROR) {
                throw std::runtime_error(
                    "Could not bind UDP receiver to 127.0.0.1:" + std::to_string(port) +
                    ". Winsock error: " + std::to_string(WSAGetLastError()));
            }

            u_long nonBlocking = 1;
            if (ioctlsocket(socket_, FIONBIO, &nonBlocking) == SOCKET_ERROR) {
                throw std::runtime_error("Could not make UDP receiver non-blocking.");
            }
        } catch (...) {
            closeResources();
            throw;
        }
    }

    ~Impl() {
        closeResources();
    }

    [[nodiscard]] std::optional<LatestReceivedFrame> receiveLatest() {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(socket_, &readSet);

        timeval timeout{};
        timeout.tv_sec = 1;

        const int ready = select(0, &readSet, nullptr, nullptr, &timeout);
        if (ready == SOCKET_ERROR) {
            throw std::runtime_error("UDP select failed. Winsock error: " +
                                     std::to_string(WSAGetLastError()));
        }
        if (ready == 0) {
            return std::nullopt;
        }

        LatestReceivedFrame result{};
        bool receivedAny = false;

        while (true) {
            OrientationFrameV1 candidate{};
            const int receivedBytes =
                recvfrom(socket_, reinterpret_cast<char*>(&candidate),
                         static_cast<int>(sizeof(candidate)), 0, nullptr, nullptr);

            if (receivedBytes == SOCKET_ERROR) {
                const int error = WSAGetLastError();
                if (error == WSAEWOULDBLOCK) {
                    break;
                }
                throw std::runtime_error("UDP receive failed. Winsock error: " +
                                         std::to_string(error));
            }

            if (receivedBytes != static_cast<int>(sizeof(candidate))) {
                continue;
            }

            if (receivedAny) {
                ++result.supersededPackets;
            }
            result.frame = candidate;
            receivedAny = true;
        }

        return receivedAny ? std::optional<LatestReceivedFrame>{result} : std::nullopt;
    }

  private:
    void closeResources() noexcept {
        if (socket_ != INVALID_SOCKET) {
            closesocket(socket_);
            socket_ = INVALID_SOCKET;
        }
        if (winsockStarted_) {
            WSACleanup();
            winsockStarted_ = false;
        }
    }

    SOCKET socket_ = INVALID_SOCKET;
    bool winsockStarted_ = false;
};

UdpOrientationReceiver::UdpOrientationReceiver(const std::uint16_t port)
    : implementation_{std::make_unique<Impl>(port)} {}

UdpOrientationReceiver::~UdpOrientationReceiver() = default;
UdpOrientationReceiver::UdpOrientationReceiver(UdpOrientationReceiver&&) noexcept = default;
UdpOrientationReceiver&
UdpOrientationReceiver::operator=(UdpOrientationReceiver&&) noexcept = default;

std::optional<LatestReceivedFrame> UdpOrientationReceiver::receiveLatest() {
    return implementation_->receiveLatest();
}

} // namespace SeatedMoCap::Realtime
