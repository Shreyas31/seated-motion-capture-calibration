#pragma once

#include "realtime_orientation_packet.h"
#include "realtime_viewer_configuration.h"
#include "support_plane_alignment.h"

#include <cstdint>
#include <memory>
#include <optional>

namespace SeatedMoCap {

/**
 * Owns the OpenSim model, orientation references, IK solver, and optional CSV writer.
 *
 * Incoming calibrated segment orientations are frame-aligned to the selected pose
 * preset before solving. The class is movable but deliberately not copyable because
 * it exclusively owns OpenSim state and solver resources.
 * @class RealtimeIkSession.
 */
class RealtimeIkSession {
  public:
    /**
     * Loads and initializes the model, preset, visualizer, and orientation IK solver.
     * @param configuration Validated viewer configuration for this session.
     * @throws std::runtime_error if model/preset compatibility, mappings, initial pose,
     * sensor count, or optional output setup is invalid.
     */
    explicit RealtimeIkSession(const RealtimeViewerConfiguration& configuration);

    /** Marks the buffered orientation stream finished and releases OpenSim resources. */
    ~RealtimeIkSession();

    RealtimeIkSession(const RealtimeIkSession&) = delete;
    RealtimeIkSession& operator=(const RealtimeIkSession&) = delete;
    RealtimeIkSession(RealtimeIkSession&&) noexcept;
    RealtimeIkSession& operator=(RealtimeIkSession&&) noexcept;

    /**
     * Returns the initial vertical pelvis shift used to place the feet on the support plane.
     * @return Vertical correction in metres in the OpenSim ground frame.
     */
    [[nodiscard]] double initialPelvisVerticalCorrection() const noexcept;

    /**
     * Returns the number of orientation frames actively tracked by the IK solver.
     * @return Number of OpenSim orientation sensors in use; expected to be 17.
     */
    [[nodiscard]] int orientationSensorsInUse() const noexcept;

    /**
     * Adds one orientation sample, tracks IK, applies optional foot anchoring, and exports angles.
     * @param frame Validated 17-segment orientation frame in protocol order.
     * @param streamTime Monotonically increasing OpenSim time in seconds.
     * @return Foot-anchor diagnostics when stationary-foot mode is enabled; otherwise std::nullopt.
     * @throws std::exception if IK tracking or joint-angle CSV writing fails.
     */
    [[nodiscard]] std::optional<FootAnchorUpdate>
    trackFrame(const Realtime::OrientationFrameV1& frame, double streamTime);

    /** Sends the current solved model state to the OpenSim visualizer. */
    void showCurrentState();

    /** Idempotently marks the buffered orientation reference as finished. */
    void finishOrientationStream();

  private:
    class Impl;
    std::unique_ptr<Impl> implementation_;
};

} // namespace SeatedMoCap
