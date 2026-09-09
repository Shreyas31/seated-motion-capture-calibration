#include "measurement_recording_workflow.h"

#include "calibration_session.h"
#include "frontend_protocol.h"
#include "measurement_csv_writer.h"
#include "output_naming.h"
#include "session_setup_workflow.h"
#include "udp_orientation_streamer.h"

#include <istream>
#include <limits>
#include <ostream>

void waitForRecordingStop(std::istream& input) {
    int command = -1;
    while (command != 0) {
        if (input >> command) {
            continue;
        }
        if (input.eof()) {
            return;
        }
        input.clear();
        input.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
        command = -1;
    }
}

MeasurementRecordingWorkflow::MeasurementRecordingWorkflow(frontend_protocol::EventWriter& frontend,
                                                           std::istream& input,
                                                           std::ostream& output,
                                                           std::ostream& errorOutput)
    : frontend_{frontend}, input_{input}, output_{output}, errorOutput_{errorOutput} {}

bool MeasurementRecordingWorkflow::run(const SessionContext& sessionContext,
                                       const CalibrationSession& calibrationSession,
                                       MeasurementCsvWriter& measurementWriter,
                                       UdpOrientationStreamer& orientationStreamer) const {

    if (!calibrationSession.hasStaticCalibration()) {
        frontend_.emitError(frontend_protocol::error::kRecording, "Static calibration is required");
        errorOutput_ << "A complete static calibration is required before recording." << std::endl;
        return false;
    }

    const auto filename =
        output_naming::nextMeasurementFilename(sessionContext.outputDirectory, sessionContext.name);
    if (!orientationStreamer.start(calibrationSession.finalOffsets(),
                                   calibrationSession.globalToSessionRotation(), "127.0.0.1",
                                   9001)) {

        frontend_.emitError(frontend_protocol::error::kRecording,
                            "Could not start OpenSim UDP streaming");
        errorOutput_ << "Could not start real-time OpenSim orientation streaming." << std::endl;
        return false;
    }

    if (!measurementWriter.startRecording(filename.string())) {
        orientationStreamer.stop();
        frontend_.emitError(frontend_protocol::error::kRecording,
                            "Could not start measurement CSV recording");
        errorOutput_ << "Measurement was not started." << std::endl;
        return false;
    }

    frontend_.emitResult(frontend_protocol::result::kRecordingStarted, filename.string());
    frontend_.emitState(frontend_protocol::state::kRecording);
    output_ << "Recording to:\n"
            << filename.string() << "\n"
            << "Enter 0 to stop this trial: ";
    waitForRecordingStop(input_);

    measurementWriter.stopRecording();
    orientationStreamer.stop();
    frontend_.emitResult(frontend_protocol::result::kRecordingStopped, filename.string());
    output_ << "Trial saved successfully. Calibration remains active.\n"
            << "You may now start another measurement trial." << std::endl;
    return true;
}
