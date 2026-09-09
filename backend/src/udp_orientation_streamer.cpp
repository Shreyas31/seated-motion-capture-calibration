#define WIN32_LEAN_AND_MEAN
#include "udp_orientation_streamer.h"

#include "packet_collector.h"
#include "realtime_orientation_packet.h"
#include "seated_calibration.h"
#include "xsens_packet_utils.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <winsock2.h>
#include <ws2tcpip.h>

namespace {

constexpr double kStreamingRateHz = 60.0;

using Eigen::Quaterniond;

Quaterniond sessionToOpenSimRotation() {
    constexpr double squareRootHalf = 0.70710678118654752440;

    // q_OC rotates backend session C into OpenSim ground O.
    return Quaterniond{squareRootHalf, -squareRootHalf, 0.0, 0.0};
}

bool finiteQuaternion(const Quaterniond& quaternion) {
    return quaternion.coeffs().allFinite() && quaternion.squaredNorm() > 1e-12;
}

} // namespace

UdpOrientationStreamer::UdpOrientationStreamer(
    const PacketCollector& packetCollector,
    const std::map<std::string, std::string>& sensorSegments)
    : packetCollector_{packetCollector}, sensorSegments_{sensorSegments} {}

UdpOrientationStreamer::~UdpOrientationStreamer() {
    stop();
}

bool UdpOrientationStreamer::start(const std::map<std::string, Quaterniond>& finalOffsets,
                                   const Quaterniond& globalToSession,
                                   const std::string& destinationAddress,
                                   std::uint16_t destinationPort) {
    stop();

    if (finalOffsets.size() != SeatedMoCap::Realtime::kSensorCount) {

        std::cerr << "UDP streaming requires all " << SeatedMoCap::Realtime::kSensorCount
                  << " final calibration offsets.\n";

        return false;
    }

    if (!finiteQuaternion(globalToSession)) {
        std::cerr << "UDP streaming received an invalid "
                     "global-to-session quaternion.\n";

        return false;
    }

    finalOffsets_ = finalOffsets;
    globalToSession_ = globalToSession.normalized();
    destinationAddress_ = destinationAddress;
    destinationPort_ = destinationPort;

    running_ = true;

    streamingThread_ = std::thread{&UdpOrientationStreamer::streamingLoop, this};

    return true;
}

void UdpOrientationStreamer::stop() {
    running_ = false;

    if (streamingThread_.joinable()) {
        streamingThread_.join();
    }
}

bool UdpOrientationStreamer::isRunning() const {
    return running_;
}

void UdpOrientationStreamer::streamingLoop() {
    WSADATA winsockData{};

    if (WSAStartup(MAKEWORD(2, 2), &winsockData) != 0) {

        std::cerr << "UDP streamer: WSAStartup failed.\n";

        running_ = false;
        return;
    }

    const SOCKET socketHandle = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (socketHandle == INVALID_SOCKET) {
        std::cerr << "UDP streamer: socket creation failed.\n";

        WSACleanup();
        running_ = false;
        return;
    }

    sockaddr_in destination{};
    destination.sin_family = AF_INET;
    destination.sin_port = htons(destinationPort_);

    if (inet_pton(AF_INET, destinationAddress_.c_str(), &destination.sin_addr) != 1) {

        std::cerr << "UDP streamer: invalid destination address: " << destinationAddress_ << '\n';

        closesocket(socketHandle);
        WSACleanup();
        running_ = false;
        return;
    }

    std::uint64_t sequence = 0;

    const Quaterniond quaternionOC = sessionToOpenSimRotation();

    const auto startTime = std::chrono::steady_clock::now();

    auto nextFrameTime = startTime;

    const auto framePeriod = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>{1.0 / kStreamingRateHz});

    while (running_) {
        nextFrameTime += framePeriod;

        const auto latestPackets = packetCollector_.latestPackets();

        SeatedMoCap::Realtime::OrientationFrameV1 frame{};

        frame.magic[0] = 'S';
        frame.magic[1] = 'M';
        frame.magic[2] = 'C';
        frame.magic[3] = '1';

        frame.version = SeatedMoCap::Realtime::kProtocolVersion;

        frame.sensorCount = SeatedMoCap::Realtime::kSensorCount;

        frame.sequence = sequence++;

        frame.timestampMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(
                                          std::chrono::steady_clock::now() - startTime)
                                          .count();

        frame.validSensorMask = 0;

        for (std::size_t index = 0; index < SeatedMoCap::Realtime::kSensorCount; ++index) {

            const std::string segment{SeatedMoCap::Realtime::kSegmentOrder[index]};

            const auto sensorIterator =
                std::find_if(sensorSegments_.begin(), sensorSegments_.end(),
                             [&segment](const auto& entry) { return entry.second == segment; });

            if (sensorIterator == sensorSegments_.end()) {
                continue;
            }

            const std::string& sensorId = sensorIterator->first;

            const auto packetIterator = latestPackets.find(sensorId);

            const auto offsetIterator = finalOffsets_.find(sensorId);

            if (packetIterator == latestPackets.end() || offsetIterator == finalOffsets_.end()) {

                continue;
            }

            const XsDataPacket& packet = packetIterator->second;

            if (!packet.containsOrientation()) {
                continue;
            }

            const Quaterniond quaternionGS = xsens_packet::orientationNwu(packet);

            const Quaterniond quaternionCS = (globalToSession_ * quaternionGS).normalized();

            const Quaterniond quaternionCB =
                SeatedCalibration::applyCalibration(quaternionCS, offsetIterator->second);

            // q_OB = q_OC * q_CB rotates anatomical segment B into OpenSim ground O.
            Quaterniond quaternionOB = quaternionOC * quaternionCB;

            if (!finiteQuaternion(quaternionOB)) {
                continue;
            }

            quaternionOB.normalize();

            frame.orientations[index] = {
                static_cast<float>(quaternionOB.w()), static_cast<float>(quaternionOB.x()),
                static_cast<float>(quaternionOB.y()), static_cast<float>(quaternionOB.z())};

            frame.validSensorMask |= (1u << index);
        }

        // Prefer a dropped frame to updating only part of the IK skeleton.
        if (frame.validSensorMask == SeatedMoCap::Realtime::kAllSensorsValidMask) {

            const int bytesSent =
                sendto(socketHandle, reinterpret_cast<const char*>(&frame),
                       static_cast<int>(sizeof(frame)), 0,
                       reinterpret_cast<const sockaddr*>(&destination), sizeof(destination));

            if (bytesSent != static_cast<int>(sizeof(frame))) {

                std::cerr << "UDP streamer: sendto failed with " << WSAGetLastError() << '\n';
            }
        }

        std::this_thread::sleep_until(nextFrameTime);
    }

    closesocket(socketHandle);
    WSACleanup();
}
