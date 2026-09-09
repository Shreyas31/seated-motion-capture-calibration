#include "orientation_stream.h"

#include <cmath>
#include <cstring>
#include <string>

namespace SeatedMoCap::Realtime {

OrientationFrameValidation validateOrientationFrame(const OrientationFrameV1& frame) {
    if (std::memcmp(frame.magic, "SMC1", 4) != 0) {
        return {false, "Invalid UDP packet magic."};
    }

    if (frame.version != kProtocolVersion) {
        return {false, "Unsupported UDP protocol version: " + std::to_string(frame.version)};
    }

    if (frame.sensorCount != kSensorCount) {
        return {false, "Unexpected sensor count: " + std::to_string(frame.sensorCount)};
    }

    if (frame.validSensorMask != kAllSensorsValidMask) {
        return {false,
                "UDP frame does not contain all " + std::to_string(kSensorCount) + " sensors."};
    }

    for (const auto& quaternion : frame.orientations) {
        const double norm = std::sqrt(quaternion.w * quaternion.w + quaternion.x * quaternion.x +
                                      quaternion.y * quaternion.y + quaternion.z * quaternion.z);

        if (!std::isfinite(norm) || std::abs(norm - 1.0) > 1e-3) {
            return {false, "UDP frame contains an invalid quaternion."};
        }
    }

    return {true, {}};
}

FrameSequenceUpdate FrameSequenceTracker::observe(const std::uint64_t sequence,
                                                  const std::uint64_t supersededPackets) noexcept {
    if (!hasAcceptedFrame_) {
        hasAcceptedFrame_ = true;
        previousSequence_ = sequence;
        return {true, 0};
    }

    if (sequence <= previousSequence_) {
        return {false, 0};
    }

    const std::uint64_t sequenceGap = sequence - previousSequence_ - 1;
    previousSequence_ = sequence;

    return {true, sequenceGap > supersededPackets ? sequenceGap - supersededPackets : 0};
}

bool FrameSequenceTracker::hasAcceptedFrame() const noexcept {
    return hasAcceptedFrame_;
}

std::uint64_t FrameSequenceTracker::previousSequence() const noexcept {
    return previousSequence_;
}

} // namespace SeatedMoCap::Realtime
