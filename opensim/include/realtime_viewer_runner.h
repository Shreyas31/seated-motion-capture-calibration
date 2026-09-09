#pragma once

#include "realtime_viewer_configuration.h"

#include <cstdint>
#include <iosfwd>

namespace SeatedMoCap {

/**
 * Summarizes packet handling, IK, visualization, and stationary-foot session outcomes.
 * @class RealtimeViewerStatistics.
 */
struct RealtimeViewerStatistics {
    std::uint64_t acceptedFrames = 0;
    std::uint64_t sequenceGaps = 0;
    std::uint64_t rejectedPackets = 0;
    std::uint64_t ikFailures = 0;
    std::uint64_t supersededFrames = 0;
    std::uint64_t visualizerFailures = 0;
    double finalPelvisRiseMetres = 0.0;
    double maximumFootAnchorResidualMetres = 0.0;
};

/**
 * Coordinates UDP reception, validation, sequence tracking, IK, visualization, and reporting.
 * @class RealtimeViewerRunner.
 */
class RealtimeViewerRunner {
  public:
    /**
     * Creates a runner using caller-owned configuration and diagnostic streams.
     * @param configuration Session settings that remain alive for the lifetime of the runner.
     * @param output Stream receiving status messages and frontend protocol events.
     * @param errorOutput Stream receiving packet-drop, IK, and visualizer diagnostics.
     */
    RealtimeViewerRunner(const RealtimeViewerConfiguration& configuration, std::ostream& output,
                         std::ostream& errorOutput);

    /**
     * Runs one viewer session until timeout, stream inactivity, or visualizer closure.
     * @return Final counters and optional stationary-foot displacement diagnostics.
     * @throws std::runtime_error when session initialization fails or no valid frame is received.
     */
    [[nodiscard]] RealtimeViewerStatistics run();

  private:
    const RealtimeViewerConfiguration& configuration_;
    std::ostream& output_;
    std::ostream& errorOutput_;
};

} // namespace SeatedMoCap
