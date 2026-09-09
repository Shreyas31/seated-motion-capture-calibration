#include "functional_calibration_workflow.h"

#include "calibration_csv_exporter.h"
#include "calibration_session.h"
#include "frontend_protocol.h"
#include "measurement_csv_writer.h"
#include "output_naming.h"
#include "packet_collector.h"
#include "sensor_mapping.h"
#include "session_setup_workflow.h"

#include <istream>
#include <limits>
#include <ostream>
#include <string>
#include <utility>

FunctionalCalibrationWorkflow::FunctionalCalibrationWorkflow(
    frontend_protocol::EventWriter& frontend, std::istream& input, std::ostream& output,
    std::ostream& errorOutput)
    : frontend_{frontend}, errorOutput_{errorOutput},
      selection_{frontend, input, output, errorOutput}, capture_{frontend, output, errorOutput},
      axes_{frontend, output, errorOutput}, refinement_{frontend, errorOutput},
      returnPose_{frontend, output, errorOutput}, finalizer_{frontend, output} {}

bool FunctionalCalibrationWorkflow::run(const SessionContext& sessionContext,
                                        const SensorMapping& sensorMapping,
                                        CalibrationSession& calibrationSession,
                                        PacketCollector& packetCollector,
                                        MeasurementCsvWriter& measurementWriter,
                                        const CalibrationCsvExporter& calibrationExporter) {

    if (!calibrationSession.hasStaticCalibration()) {
        frontend_.emitError(frontend_protocol::error::kFunctionalCalibration,
                            "Static calibration is required");
        errorOutput_ << "Perform and pass a static calibration before "
                     << "functional refinement." << std::endl;
        return false;
    }

    const auto selectedJoint = selection_.collect();
    if (!selectedJoint) {
        return false;
    }
    const calibration::JointCalibrationDefinition& joint = *selectedJoint;

    const auto capture =
        capture_.capture(joint, sensorMapping, calibrationSession, packetCollector);
    if (!capture) {
        return false;
    }

    const auto axisAnalysis = axes_.analyze(joint, *capture, calibrationSession);
    if (!axisAnalysis) {
        return false;
    }

    auto candidate = refinement_.solve(joint, *capture, *axisAnalysis, calibrationSession);
    if (!candidate) {
        return false;
    }

    const auto returnPose = returnPose_.captureValidateAndCommit(
        joint, *capture, std::move(*candidate), calibrationSession, packetCollector);
    if (!returnPose) {
        return false;
    }

    finalizer_.finalize(joint, *capture, *axisAnalysis, *returnPose, sessionContext, sensorMapping,
                        calibrationSession, measurementWriter, calibrationExporter);
    return true;
}

FunctionalCalibrationSelectionWorkflow::FunctionalCalibrationSelectionWorkflow(
    frontend_protocol::EventWriter& frontend, std::istream& input, std::ostream& output,
    std::ostream& errorOutput)
    : frontend_{frontend}, input_{input}, output_{output}, errorOutput_{errorOutput} {}

std::optional<calibration::JointCalibrationDefinition>
FunctionalCalibrationSelectionWorkflow::collect() const {
    frontend_.emitState(frontend_protocol::state::kJoint);
    output_ << "\nSelect a joint for paired functional calibration:" << std::endl;

    for (std::size_t index = 0; index < calibration::JOINT_CALIBRATIONS.size(); ++index) {

        output_ << (index + 1) << ". " << calibration::JOINT_CALIBRATIONS[index].displayName
                << std::endl;
    }
    output_ << "Choice: ";

    std::size_t choice = 0;
    if (!(input_ >> choice) || choice == 0 || choice > calibration::JOINT_CALIBRATIONS.size()) {

        input_.clear();
        input_.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
        errorOutput_ << "Invalid joint choice." << std::endl;
        return std::nullopt;
    }

    return calibration::JOINT_CALIBRATIONS[choice - 1];
}

FunctionalCalibrationFinalizer::FunctionalCalibrationFinalizer(
    frontend_protocol::EventWriter& frontend, std::ostream& output)
    : frontend_{frontend}, output_{output} {}

void FunctionalCalibrationFinalizer::finalize(
    const calibration::JointCalibrationDefinition& joint,
    const FunctionalCalibrationCapture& capture, const FunctionalAxisAnalysis& axisAnalysis,
    const FunctionalReturnPoseResult& returnPose, const SessionContext& sessionContext,
    const SensorMapping& sensorMapping, CalibrationSession& calibrationSession,
    MeasurementCsvWriter& measurementWriter,
    const CalibrationCsvExporter& calibrationExporter) const {

    measurementWriter.setCalibrationOffsets(calibrationSession.finalOffsets());

    const std::string jointFileLabel = output_naming::safeFilenamePart(joint.displayName);
    const std::size_t captureNumber =
        calibrationSession.nextFunctionalCaptureNumber(jointFileLabel);
    const auto filename = output_naming::functionalCalibrationFilename(
        sessionContext.outputDirectory, sessionContext.name, jointFileLabel, captureNumber);
    const auto& sensorToSegment = sensorMapping.sensorToSegment();

    const bool gravitySaved = calibrationExporter.saveCapture(
        filename.string(), sessionContext.name, "Functional", "StaticGravity", joint.displayName,
        "Reference static pose", capture.gravityPackets, sensorToSegment,
        calibrationSession.staticOffsets(), calibrationSession.finalOffsets(),
        calibrationSession.activePoseTargets(), calibrationSession.globalToSessionRotation(),
        false);

    std::string dynamicPhase = "DynamicFlexionExtension";

    if (axisAnalysis.isPronationSupination) {
        dynamicPhase = "DynamicPronationSupination";
    } else if (axisAnalysis.isShoulderAbductionAdduction) {
        dynamicPhase = "DynamicShoulderAbductionAdduction";
    }

    const bool dynamicSaved = calibrationExporter.saveCapture(
        filename.string(), sessionContext.name, "Functional", dynamicPhase, joint.displayName,
        "Moving", capture.dynamicPackets, sensorToSegment, calibrationSession.staticOffsets(),
        calibrationSession.finalOffsets(), calibrationSession.activePoseTargets(),
        calibrationSession.globalToSessionRotation(), true);

    const bool returnPoseSaved = calibrationExporter.saveCapture(
        filename.string(), sessionContext.name, "Functional", "ReturnStaticValidation",
        joint.displayName, "Return to reference static pose", returnPose.packets, sensorToSegment,
        calibrationSession.staticOffsets(), calibrationSession.finalOffsets(),
        calibrationSession.activePoseTargets(), calibrationSession.globalToSessionRotation(), true);

    if (gravitySaved && dynamicSaved && returnPoseSaved) {
        output_ << "Functional calibration data saved to " << filename.string() << std::endl;
    }

    output_ << "\nCombined functional calibration applied to " << joint.displayName << "\n"
            << "Proximal observation count: " << returnPose.proximalRefinement.observationCount
            << "\n"
            << "Proximal correction from static: "
            << returnPose.proximalRefinement.correctionDegrees << " degrees\n"
            << "Proximal weighted RMS residual: "
            << returnPose.proximalRefinement.weightedRmsResidualDegrees
            << " degrees\nProximal maximum residual: "
            << returnPose.proximalRefinement.maximumResidualDegrees
            << " degrees\nDistal observation count: "
            << returnPose.distalRefinement.observationCount << "\n"
            << "Distal correction from static: " << returnPose.distalRefinement.correctionDegrees
            << " degrees\n"
            << "Distal weighted RMS residual: "
            << returnPose.distalRefinement.weightedRmsResidualDegrees
            << " degrees\nDistal maximum residual: "
            << returnPose.distalRefinement.maximumResidualDegrees
            << " degrees\nSynchronized packets: "
            << axisAnalysis.relativeSamples.synchronizedPackets << std::endl;

    frontend_.emitResult(frontend_protocol::result::kFunctionalCalibration, joint.displayName);
    output_ << "Weighted Wahba refinement applied to " << joint.displayName
            << ".\nSynchronized samples: " << axisAnalysis.relativeSamples.synchronizedPackets
            << std::endl;
}
