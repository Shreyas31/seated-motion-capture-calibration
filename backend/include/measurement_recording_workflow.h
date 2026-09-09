#pragma once

#include <iosfwd>

class CalibrationSession;
class MeasurementCsvWriter;
class UdpOrientationStreamer;
struct SessionContext;
namespace frontend_protocol {
class EventWriter;
}

/**
 * Blocks until zero is entered or the controlling input stream closes.
 * @param input Stream carrying the frontend's recording-stop command.
 */
void waitForRecordingStop(std::istream& input);

/**
 * Coordinates one measurement CSV trial and its real-time OpenSim UDP stream.
 * @class MeasurementRecordingWorkflow.
 */
class MeasurementRecordingWorkflow {
  public:
    /**
     * Creates a recording workflow using injected protocol and console streams.
     * @param frontend Writer for recording state and result events.
     * @param input Stream used to receive the stop command.
     * @param output Stream used for normal recording messages.
     * @param errorOutput Stream used for startup and recording errors.
     */
    MeasurementRecordingWorkflow(frontend_protocol::EventWriter& frontend, std::istream& input,
                                 std::ostream& output, std::ostream& errorOutput);

    /**
     * Records one trial while streaming the same calibrated orientations to OpenSim.
     * @param sessionContext Session name and output directory used for the CSV path.
     * @param calibrationSession Accepted calibration offsets and session transformation.
     * @param measurementWriter CSV writer receiving asynchronous Xsens packets.
     * @param orientationStreamer UDP streamer feeding the real-time OpenSim viewer.
     * @return True after a trial starts and stops normally; false if prerequisites or startup fail.
     */
    bool run(const SessionContext& sessionContext, const CalibrationSession& calibrationSession,
             MeasurementCsvWriter& measurementWriter,
             UdpOrientationStreamer& orientationStreamer) const;

  private:
    frontend_protocol::EventWriter& frontend_;
    std::istream& input_;
    std::ostream& output_;
    std::ostream& errorOutput_;
};
