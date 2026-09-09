#include "acquisition_workflow.h"

#include "application_menu.h"
#include "awinda_system.h"
#include "calibration_csv_exporter.h"
#include "calibration_session.h"
#include "frontend_protocol.h"
#include "functional_calibration_workflow.h"
#include "measurement_csv_writer.h"
#include "measurement_recording_workflow.h"
#include "packet_collector.h"
#include "sensor_mapping.h"
#include "session_setup_workflow.h"
#include "static_calibration_workflow.h"
#include "udp_orientation_streamer.h"

#include <ostream>

AcquisitionWorkflow::AcquisitionWorkflow(frontend_protocol::EventWriter& frontend,
                                         std::istream& input, std::ostream& output,
                                         std::ostream& errorOutput)
    : frontend_{frontend}, input_{input}, output_{output}, errorOutput_{errorOutput} {}

int AcquisitionWorkflow::run(AwindaSystem& awinda, const SensorMapping& sensorMapping,
                             const SessionContext& sessionContext) const {

    const auto& sensorToSegment = sensorMapping.sensorToSegment();
    MeasurementCsvWriter measurementWriter{sensorToSegment};
    PacketCollector packetCollector{measurementWriter};
    UdpOrientationStreamer orientationStreamer{packetCollector, sensorToSegment};
    MeasurementRecordingWorkflow recording{frontend_, input_, output_, errorOutput_};
    CalibrationCsvExporter calibrationExporter;

    // Registration must be destroyed before packetCollector and AwindaSystem.
    // Its position after the collector guarantees that ordering during normal
    // return and exception unwinding.
    auto callbackRegistration = awinda.registerMtwCallback(packetCollector);

    CalibrationSession calibrationSession;
    StaticCalibrationWorkflow staticCalibration{frontend_, output_, errorOutput_};
    FunctionalCalibrationWorkflow functionalCalibration{frontend_, input_, output_, errorOutput_};
    ApplicationMenu menu{frontend_, input_, output_};

    while (true) {
        const auto action = menu.collect();
        if (!action) {
            continue;
        }

        const auto poseChoice = staticPoseChoice(*action);
        if (poseChoice) {
            const StaticPoseSelection pose = buildStaticPoseSelection(*poseChoice);
            staticCalibration.run(pose, sessionContext, sensorMapping, calibrationSession,
                                  packetCollector, measurementWriter, calibrationExporter);
            continue;
        }

        if (*action == ApplicationAction::FunctionalCalibration) {
            functionalCalibration.run(sessionContext, sensorMapping, calibrationSession,
                                      packetCollector, measurementWriter, calibrationExporter);
            continue;
        }

        if (*action == ApplicationAction::RecordMeasurement) {
            recording.run(sessionContext, calibrationSession, measurementWriter,
                          orientationStreamer);
            continue;
        }

        if (*action == ApplicationAction::Exit) {
            frontend_.emitState(frontend_protocol::state::kExiting);
            output_ << "Exiting program." << std::endl;
            return 0;
        }
    }
}
