#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace SeatedMoCap::Realtime {

/** Binary protocol version understood by the backend and OpenSim viewer. */
inline constexpr std::uint16_t kProtocolVersion = 1;

/** Number of segment orientations carried by every complete frame. */
inline constexpr std::uint16_t kSensorCount = 17;

/** Default loopback UDP port used by the orientation stream. */
inline constexpr std::uint16_t kDefaultPort = 9001;

/** Bit mask indicating that every expected sensor orientation is valid. */
inline constexpr std::uint32_t kAllSensorsValidMask = (1u << kSensorCount) - 1u;

/**
     * Defines the protocol-fixed segment order of the orientation array.
     *
     * This
 * order must remain identical to the OpenSim segment-model mapping.
     */
inline constexpr std::array<std::string_view, kSensorCount> kSegmentOrder{
    "Pelvis",         "Sternum",        "Head",

    "Right_Shoulder", "Right_Upperarm", "Right_Forearm", "Right_Hand",

    "Left_Shoulder",  "Left_Upperarm",  "Left_Forearm",  "Left_Hand",

    "Right_Upperleg", "Right_Lowerleg", "Right_Foot",

    "Left_Upperleg",  "Left_Lowerleg",  "Left_Foot"};

#pragma pack(push, 1)

/**
     * Stores a single-precision unit quaternion in scalar-first order.
     * @class
 * QuaternionWxyzFloat.
     */
struct QuaternionWxyzFloat {
    float w;
    float x;
    float y;
    float z;
};

/**
     * Defines one packed version-1 UDP datagram containing all 17 orientations.
     *
     *
 * Orientations follow kSegmentOrder, timestamps are expressed in microseconds,
     * and the four
 * magic bytes contain the ASCII literal `SMC1`.
     * @class OrientationFrameV1.
     */
struct OrientationFrameV1 {
    // Literal ASCII bytes: S M C 1
    char magic[4];

    std::uint16_t version;
    std::uint16_t sensorCount;

    std::uint64_t sequence;
    std::int64_t timestampMicroseconds;

    std::uint32_t validSensorMask;

    QuaternionWxyzFloat orientations[kSensorCount];
};

#pragma pack(pop)

/** Expected byte size of one packed OrientationFrameV1 datagram. */
inline constexpr std::size_t kExpectedPacketSize =
    4 + sizeof(std::uint16_t) * 2 + sizeof(std::uint64_t) + sizeof(std::int64_t) +
    sizeof(std::uint32_t) + sizeof(QuaternionWxyzFloat) * kSensorCount;

static_assert(sizeof(OrientationFrameV1) == kExpectedPacketSize);

} // namespace SeatedMoCap::Realtime
