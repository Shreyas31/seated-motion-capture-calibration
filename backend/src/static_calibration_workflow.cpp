#include "static_calibration_workflow.h"

#include "angular_uncertainty.h"
#include "calibration_csv_exporter.h"
#include "calibration_sample_analysis.h"
#include "calibration_session.h"
#include "frontend_protocol.h"
#include "measurement_csv_writer.h"
#include "output_naming.h"
#include "packet_collector.h"
#include "seated_calibration.h"
#include "sensor_mapping.h"
#include "session_setup_workflow.h"

#include <Eigen>
#include <cmath>
#include <map>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

StaticCalibrationWorkflow::StaticCalibrationWorkflow(frontend_protocol::EventWriter& frontend,
                                                     std::ostream& output,
                                                     std::ostream& errorOutput,
                                                     const std::chrono::seconds preparationSeconds,
                                                     const std::chrono::seconds captureSeconds)
    : frontend_{frontend}, output_{output}, errorOutput_{errorOutput},
      preparationSeconds_{preparationSeconds}, captureSeconds_{captureSeconds} {}

bool StaticCalibrationWorkflow::run(const StaticPoseSelection& selection,
                                    const SessionContext& sessionContext,
                                    const SensorMapping& sensorMapping,
                                    CalibrationSession& calibrationSession,
                                    PacketCollector& packetCollector,
                                    MeasurementCsvWriter& measurementWriter,
                                    const CalibrationCsvExporter& calibrationExporter) const {

    calibrationSession.selectPose(selection.pose);

    frontend_.emitGuidance(frontend_protocol::guidance::kStaticPreparation, selection.description);
    output_ << "\nHold patient in " << selection.description << " pose for static calibration."
            << std::endl;
    output_ << "Ensure the marked +X axes of the head sensor points "
            << "approximately toward the patient's front." << std::endl;
    output_ << "Use the chair/backrest/foot supports to make the pose "
            << "as reproducible as the patient's mobility permits." << std::endl;
    output_ << "Starting capture in " << preparationSeconds_.count() << " seconds..." << std::endl;

    frontend_.runGuidedCountdown("STATIC", static_cast<int>(preparationSeconds_.count()));
    frontend_.emitGuidance(frontend_protocol::guidance::kStaticCapture,
                           std::to_string(captureSeconds_.count()));

    packetCollector.clearCalibrationBuffers();
    output_ << "RECORDING STATIC DATA FOR " << captureSeconds_.count() << " SECONDS... DO NOT MOVE."
            << std::endl;
    std::this_thread::sleep_for(captureSeconds_);
    auto staticPackets = packetCollector.takeCalibrationPackets();
    frontend_.emitGuidance(frontend_protocol::guidance::kStaticProcessing, selection.description);

    const auto sessionRotation =
        calibration_analysis::estimateCalibrationRelativeFrame(staticPackets, sensorMapping);
    if (!sessionRotation) {
        calibrationSession.invalidateStaticCalibration();
        errorOutput_ << "Could not define the session frame from the head sensor. "
                     << "Check their mounting and data." << std::endl;
        return false;
    }

    const auto& sensorToSegment = sensorMapping.sensorToSegment();
    std::map<std::string, Eigen::Quaterniond> candidateOffsets;
    std::map<std::string, Eigen::Vector3d> candidateBiases;
    std::map<std::string, GravityDirectionEstimate> candidateGravityEstimates;
    std::vector<double> acceptedGravityUncertainties;
    std::size_t calibratedSensorCount = 0;

    for (const auto& [sensorId, segmentName] : sensorToSegment) {
        const auto packet = staticPackets.find(sensorId);
        if (packet == staticPackets.end()) {
            errorOutput_ << "No static packets for " << segmentName << "." << std::endl;
            continue;
        }

        const auto orientations =
            calibration_analysis::extractStationaryOrientations(packet->second);
        const auto averageOrientation = SeatedCalibration::averageQuaternions(orientations);
        std::optional<Eigen::Quaterniond> q_CS_average;
        if (averageOrientation) {
            q_CS_average = (*sessionRotation * *averageOrientation).normalized();
        }

        const auto gyroBias = calibration_analysis::estimateStationaryGyroBias(packet->second);

        const GravityDirectionEstimate gravityEstimate = SeatedCalibration::computeGravityDirection(
            calibration_analysis::extractAccelerations(packet->second));

        if (!q_CS_average || !gyroBias || !gravityEstimate.valid) {

            errorOutput_ << "Rejected static data for " << segmentName
                         << " (orientation, gyro bias or gravity "
                            "uncertainty unavailable).";

            if (!gravityEstimate.valid) {
                errorOutput_ << " Gravity: " << gravityEstimate.message;
            }

            errorOutput_ << std::endl;
            continue;
        }

        const Eigen::Quaterniond& q_CB_target =
            calibrationSession.activePoseTargets().at(segmentName);
        candidateOffsets[sensorId] =
            SeatedCalibration::computeStaticOffset(*q_CS_average, q_CB_target);
        candidateBiases[sensorId] = *gyroBias;
        candidateGravityEstimates[sensorId] = gravityEstimate;
        acceptedGravityUncertainties.push_back(gravityEstimate.angularUncertaintyRadians);

        const double radiansToDegrees = 180.0 / std::acos(-1.0);
        output_ << "Static gravity uncertainty — " << segmentName << ": "
                << gravityEstimate.angularUncertaintyRadians * radiansToDegrees << " degrees from "
                << gravityEstimate.usedBlocks << " blocks." << std::endl;

        ++calibratedSensorCount;
    }

    if (calibratedSensorCount != sensorToSegment.size()) {
        calibrationSession.invalidateStaticCalibration();
        frontend_.emitError(frontend_protocol::error::kStaticCalibration,
                            "Expected " + std::to_string(sensorToSegment.size()) +
                                " sensors but accepted " + std::to_string(calibratedSensorCount));
        errorOutput_ << "Static calibration rejected: expected " << sensorToSegment.size()
                     << " sensors but accepted " << calibratedSensorCount << "." << std::endl;
        output_ << "Static calibration completed for " << calibratedSensorCount << " sensors."
                << std::endl;
        return false;
    }

    const auto gravityFloor = calibration_statistics::median(acceptedGravityUncertainties);

    if (!gravityFloor || !std::isfinite(*gravityFloor) || *gravityFloor < 0.0) {

        calibrationSession.invalidateStaticCalibration();

        frontend_.emitError(frontend_protocol::error::kStaticCalibration,
                            "Could not calculate session gravity "
                            "uncertainty floor");

        errorOutput_ << "Static calibration rejected: "
                     << "gravity uncertainty floor is unavailable." << std::endl;

        return false;
    }

    const double radiansToDegrees = 180.0 / std::acos(-1.0);

    output_ << "Session gravity uncertainty floor: " << *gravityFloor * radiansToDegrees
            << " degrees, calculated as the median of " << acceptedGravityUncertainties.size()
            << " sensor uncertainties." << std::endl;

    calibrationSession.commitStaticCalibration(
        std::move(candidateOffsets), std::move(candidateBiases),
        std::move(candidateGravityEstimates), *gravityFloor, *sessionRotation);
    measurementWriter.setGlobalToSessionRotation(calibrationSession.globalToSessionRotation());
    measurementWriter.setStaticCalibrationOffsets(calibrationSession.staticOffsets());
    measurementWriter.setCalibrationOffsets(calibrationSession.finalOffsets());

    const std::size_t captureNumber = calibrationSession.nextStaticCaptureNumber();
    const auto filename = output_naming::staticCalibrationFilename(
        sessionContext.outputDirectory, sessionContext.name, selection.fileLabel, captureNumber);

    if (calibrationExporter.saveCapture(filename.string(), sessionContext.name, "Static",
                                        "StaticPose", "", selection.description, staticPackets,
                                        sensorToSegment, calibrationSession.staticOffsets(),
                                        calibrationSession.finalOffsets(),
                                        calibrationSession.activePoseTargets(),
                                        calibrationSession.globalToSessionRotation(), false)) {

        frontend_.emitResult(frontend_protocol::result::kStaticCalibration,
                             selection.fileLabel + ":" + std::to_string(calibratedSensorCount));
        output_ << "Static calibration data saved to " << filename.string() << std::endl;
    }

    output_ << "Static calibration completed for " << calibratedSensorCount << " sensors."
            << std::endl;
    return true;
}

namespace {

PoseType poseFromChoice(const int choice) {
    switch (choice) {
    case 1:
        return PoseType::Chair;
    case 2:
        return PoseType::Bed;
    default:
        throw std::invalid_argument("Static pose choice must be 1 or 2.");
    }
}

std::string poseDescription(const PoseType pose) {
    switch (pose) {
    case PoseType::Chair:
        return "90-degree-leg chair";
    case PoseType::Bed:
        return "straight-leg bed";
    }
    throw std::invalid_argument("Unsupported static pose.");
}

std::string poseFileLabel(const PoseType pose) {
    switch (pose) {
    case PoseType::Chair:
        return "Chair";
    case PoseType::Bed:
        return "Bed";
    }
    throw std::invalid_argument("Unsupported static pose.");
}

} // namespace

StaticPoseSelection buildStaticPoseSelection(const int poseChoice) {
    const PoseType pose = poseFromChoice(poseChoice);
    return StaticPoseSelection{pose, poseDescription(pose) + " with palms facing each other",
                               poseFileLabel(pose) + "_PalmsTogether"};
}
