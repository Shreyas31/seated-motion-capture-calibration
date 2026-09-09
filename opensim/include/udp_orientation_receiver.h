#pragma once

#include "realtime_orientation_packet.h"

#include <cstdint>
#include <memory>
#include <optional>

namespace SeatedMoCap::Realtime {

/**
 * Couples the newest complete UDP orientation frame with the packets it superseded.
 * @class LatestReceivedFrame.
 */
struct LatestReceivedFrame {
    OrientationFrameV1 frame;
    std::uint64_t supersededPackets = 0;
};

/**
 * Receives the backend orientation stream from a non-blocking loopback UDP socket.
 * @class UdpOrientationReceiver.
 */
class UdpOrientationReceiver {
  public:
    /**
     * Opens a receiver bound to the loopback interface on the requested port.
     * @param port UDP port on 127.0.0.1 from which frames are received.
     * @throws std::runtime_error if Winsock or socket configuration fails.
     */
    explicit UdpOrientationReceiver(std::uint16_t port);

    /** Closes the UDP socket and releases the associated Winsock resources. */
    ~UdpOrientationReceiver();

    UdpOrientationReceiver(const UdpOrientationReceiver&) = delete;
    UdpOrientationReceiver& operator=(const UdpOrientationReceiver&) = delete;
    UdpOrientationReceiver(UdpOrientationReceiver&&) noexcept;
    UdpOrientationReceiver& operator=(UdpOrientationReceiver&&) noexcept;

    /**
     * Waits for up to one second and drains all currently queued datagrams.
     *
     * Datagrams with an unexpected byte count are discarded. When multiple complete
     * datagrams are queued, only the newest is returned to minimise visualization lag.
     * @return The newest complete frame and number of older complete packets superseded,
     * or std::nullopt when no datagram arrives before the timeout.
     * @throws std::runtime_error if the socket wait or receive operation fails.
     */
    [[nodiscard]] std::optional<LatestReceivedFrame> receiveLatest();

  private:
    class Impl;
    std::unique_ptr<Impl> implementation_;
};

} // namespace SeatedMoCap::Realtime
