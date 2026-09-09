#pragma once

#include "realtime_orientation_packet.h"

#include <cstdint>
#include <string>

namespace SeatedMoCap::Realtime {

/**
 * Describes whether a received orientation frame satisfies the protocol contract.
 * @class OrientationFrameValidation.
 */
struct OrientationFrameValidation {
    bool valid = false;
    std::string errorMessage;
};

/**
 * Validates packet identity, version, sensor completeness, and quaternion norms.
 * @param frame Packed UDP orientation frame to validate.
 * @return Validation result with an explanatory message when invalid.
 */
[[nodiscard]] OrientationFrameValidation validateOrientationFrame(const OrientationFrameV1& frame);

/**
 * Reports whether a sequence number was accepted and any unexplained packet gap.
 * @class FrameSequenceUpdate.
 */
struct FrameSequenceUpdate {
    bool accepted = false;
    std::uint64_t unaccountedGap = 0;
};

/**
 * Rejects duplicate or out-of-order frames and accounts for known superseded packets.
 * @class FrameSequenceTracker.
 */
class FrameSequenceTracker {
  public:
    /**
     * Observes the next UDP sequence number.
     * @param sequence Sequence number of the newest received frame.
     * @param supersededPackets Complete older datagrams deliberately drained by the receiver.
     * @return Acceptance status and the gap not explained by superseded packets.
     */
    [[nodiscard]] FrameSequenceUpdate observe(std::uint64_t sequence,
                                              std::uint64_t supersededPackets) noexcept;

    /**
     * Reports whether at least one sequence number has been accepted.
     * @return True after the first accepted observation; otherwise false.
     */
    [[nodiscard]] bool hasAcceptedFrame() const noexcept;

    /**
     * Returns the most recently accepted sequence number.
     * @return Last accepted sequence, or zero before the first accepted observation.
     */
    [[nodiscard]] std::uint64_t previousSequence() const noexcept;

  private:
    bool hasAcceptedFrame_ = false;
    std::uint64_t previousSequence_ = 0;
};

} // namespace SeatedMoCap::Realtime
