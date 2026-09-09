#include "realtime_viewer_runner.h"

#include "orientation_stream.h"
#include "realtime_ik_session.h"
#include "udp_orientation_receiver.h"

#include <algorithm>
#include <chrono>
#include <ostream>
#include <stdexcept>

namespace SeatedMoCap {
namespace {

constexpr auto kStreamInactivityTimeout = std::chrono::seconds{3};
constexpr auto kVisualUpdateInterval = std::chrono::milliseconds{33};
constexpr double kFootAnchorWarningThresholdMetres = 0.03;

} // namespace

RealtimeViewerRunner::RealtimeViewerRunner(const RealtimeViewerConfiguration& configuration,
                                           std::ostream& output, std::ostream& errorOutput)
    : configuration_{configuration}, output_{output}, errorOutput_{errorOutput} {}

RealtimeViewerStatistics RealtimeViewerRunner::run() {
    RealtimeIkSession ikSession{configuration_};

    output_ << "Initial " << configuration_.poseName << " pose raised by "
            << ikSession.initialPelvisVerticalCorrection()
            << " m to place the feet on the support plane." << std::endl;

    if (!configuration_.jointAngleOutputPath.empty()) {
        output_ << "Joint angles will be written to:\n"
                << configuration_.jointAngleOutputPath.string() << std::endl;
    }

    if (configuration_.stationaryFeetEnabled()) {
        output_ << "Stationary-foot translation enabled for " << configuration_.poseName << " pose."
                << std::endl;
    } else {
        output_ << "Stationary-foot translation disabled." << std::endl;
    }

    output_ << "\nInitial " << configuration_.poseName << " pose loaded.\n"
            << "Orientation sensors in use: " << ikSession.orientationSensorsInUse() << '\n';
    ikSession.showCurrentState();

    Realtime::UdpOrientationReceiver receiver{Realtime::kDefaultPort};
    output_ << "\nFRONTEND|VIEWER|READY|" << configuration_.poseName << std::endl;
    output_ << "\nListening for calibrated backend frames on 127.0.0.1:" << Realtime::kDefaultPort
            << " for up to " << configuration_.listeningDurationSeconds
            << " seconds.\nStart data recording in the backend now.\n";

    RealtimeViewerStatistics statistics;
    Realtime::FrameSequenceTracker sequenceTracker;
    std::int64_t firstTimestampMicroseconds = 0;
    double latestPelvisHeight = 0.0;
    double initialPelvisHeight = 0.0;
    bool capturedInitialPelvisHeight = false;
    bool footAnchorWarningShown = false;
    bool visualizerEnabled = true;

    const auto listeningStart = std::chrono::steady_clock::now();
    auto lastVisualUpdate = listeningStart;
    auto lastValidPacketTime = listeningStart;

    while (std::chrono::steady_clock::now() - listeningStart <
           std::chrono::seconds{configuration_.listeningDurationSeconds}) {
        const auto received = receiver.receiveLatest();

        if (!received) {
            const auto now = std::chrono::steady_clock::now();
            if (sequenceTracker.hasAcceptedFrame() &&
                now - lastValidPacketTime >= kStreamInactivityTimeout) {
                output_ << "\nFRONTEND|VIEWER|STREAM_ENDED|INACTIVITY" << std::endl;
                output_ << "No valid UDP frames received for " << kStreamInactivityTimeout.count()
                        << " seconds; ending viewer session." << std::endl;
                break;
            }

            if (!sequenceTracker.hasAcceptedFrame()) {
                output_ << "Waiting for the backend to start recording...\n";
            }
            continue;
        }

        const auto& frame = received->frame;
        statistics.supersededFrames += received->supersededPackets;

        const auto validation = Realtime::validateOrientationFrame(frame);
        if (!validation.valid) {
            errorOutput_ << "[DROP] " << validation.errorMessage << '\n';
            ++statistics.rejectedPackets;
            continue;
        }
        lastValidPacketTime = std::chrono::steady_clock::now();

        const bool hadAcceptedFrame = sequenceTracker.hasAcceptedFrame();
        const auto sequenceUpdate =
            sequenceTracker.observe(frame.sequence, received->supersededPackets);
        if (!sequenceUpdate.accepted) {
            errorOutput_ << "[DROP] Out-of-order or duplicate frame: " << frame.sequence << '\n';
            ++statistics.rejectedPackets;
            continue;
        }
        statistics.sequenceGaps += sequenceUpdate.unaccountedGap;

        if (!hadAcceptedFrame) {
            firstTimestampMicroseconds = frame.timestampMicroseconds;
        }

        const double streamTime =
            0.001 + static_cast<double>(frame.timestampMicroseconds - firstTimestampMicroseconds) /
                        1'000'000.0;

        bool ikSucceeded = false;
        try {
            const auto anchorUpdate = ikSession.trackFrame(frame, streamTime);
            if (anchorUpdate) {
                statistics.maximumFootAnchorResidualMetres =
                    (std::max)(statistics.maximumFootAnchorResidualMetres,
                               anchorUpdate->maximumResidual);
                latestPelvisHeight = anchorUpdate->pelvisY;

                if (!capturedInitialPelvisHeight) {
                    initialPelvisHeight = latestPelvisHeight;
                    capturedInitialPelvisHeight = true;
                }
                statistics.finalPelvisRiseMetres = latestPelvisHeight - initialPelvisHeight;

                if (anchorUpdate->maximumResidual > kFootAnchorWarningThresholdMetres &&
                    !footAnchorWarningShown) {
                    errorOutput_ << "[FOOT ANCHOR WARNING] Left/right foot constraints disagree "
                                    "by more than 30 mm. The participant may have stepped, or "
                                    "lower-limb IK may be inconsistent."
                                 << std::endl;
                    footAnchorWarningShown = true;
                }
            }

            ++statistics.acceptedFrames;
            ikSucceeded = true;
        } catch (const std::exception& error) {
            ++statistics.ikFailures;
            errorOutput_ << "[IK DROP] Sequence " << frame.sequence << ": " << error.what() << '\n';
        }

        if (ikSucceeded && visualizerEnabled) {
            const auto now = std::chrono::steady_clock::now();
            if (now - lastVisualUpdate >= kVisualUpdateInterval) {
                try {
                    ikSession.showCurrentState();
                    lastVisualUpdate = now;
                } catch (const std::exception& error) {
                    ++statistics.visualizerFailures;
                    visualizerEnabled = false;
                    errorOutput_ << "[VIEWER CLOSED] Visualization window was closed: "
                                 << error.what() << '\n';
                    output_ << "FRONTEND|VIEWER|CLOSED|USER" << std::endl;
                    break;
                }
            }
        }

        if (statistics.acceptedFrames > 0 && statistics.acceptedFrames % 40 == 0) {
            output_ << "Processed " << statistics.acceptedFrames
                    << " frames | UDP sequence gaps: " << statistics.sequenceGaps
                    << " | rejected packets: " << statistics.rejectedPackets
                    << " | IK failures: " << statistics.ikFailures
                    << " | superseded frames: " << statistics.supersededFrames
                    << " | visualizer failures: " << statistics.visualizerFailures;

            if (configuration_.stationaryFeetEnabled()) {
                output_ << " | pelvis rise: " << 1000.0 * statistics.finalPelvisRiseMetres
                        << " mm | foot residual: "
                        << 1000.0 * statistics.maximumFootAnchorResidualMetres << " mm";
            }
            output_ << '\n';
        }
    }

    ikSession.finishOrientationStream();

    output_ << "\nReal-time session finished.\n"
            << "Accepted frames: " << statistics.acceptedFrames
            << "\nUDP sequence gaps: " << statistics.sequenceGaps
            << "\nRejected packets: " << statistics.rejectedPackets
            << "\nIK failures: " << statistics.ikFailures
            << "\nSuperseded frames: " << statistics.supersededFrames
            << "\nVisualizer failures: " << statistics.visualizerFailures;

    if (configuration_.stationaryFeetEnabled()) {
        output_ << "\nEstimated final pelvis rise: " << 1000.0 * statistics.finalPelvisRiseMetres
                << " mm\nMaximum stationary-foot residual: "
                << 1000.0 * statistics.maximumFootAnchorResidualMetres << " mm";
    }
    output_ << '\n';

    if (statistics.acceptedFrames == 0) {
        throw std::runtime_error("No valid backend UDP frames were received.");
    }

    return statistics;
}

} // namespace SeatedMoCap
