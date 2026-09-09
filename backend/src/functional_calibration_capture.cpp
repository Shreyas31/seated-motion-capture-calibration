#include "functional_calibration_capture.h"

#include "calibration_sample_analysis.h"
#include "calibration_session.h"
#include "frontend_protocol.h"
#include "packet_collector.h"
#include "seated_calibration.h"
#include "sensor_mapping.h"

#include <algorithm>
#include <cmath>
#include <ostream>
#include <thread>
#include <utility>

namespace {

std::optional<std::string>
sensorForSegment(const std::map<std::string, std::string>& sensorToSegment,
                 const std::string& requiredSegment) {

    const auto sensor = std::find_if(sensorToSegment.begin(), sensorToSegment.end(),
                                     [&requiredSegment](const auto& assignment) {
                                         return assignment.second == requiredSegment;
                                     });

    return sensor == sensorToSegment.end() ? std::nullopt
                                           : std::optional<std::string>{sensor->first};
}

} // namespace

std::optional<FunctionalSensorPair>
resolveFunctionalSensorPair(const calibration::JointCalibrationDefinition& joint,
                            const std::map<std::string, std::string>& sensorToSegment,
                            const std::map<std::string, Eigen::Quaterniond>& staticOffsets,
                            const std::map<std::string, Eigen::Quaterniond>& finalOffsets,
                            const std::map<std::string, Eigen::Vector3d>& gyroBiases) {

    const auto proximal = sensorForSegment(sensorToSegment, joint.proximalSegment);
    const auto distal = sensorForSegment(sensorToSegment, joint.distalSegment);
    if (!proximal || !distal) {
        return std::nullopt;
    }

    const auto calibrated = [&](const std::string& sensorId) {
        return staticOffsets.contains(sensorId) && finalOffsets.contains(sensorId) &&
               gyroBiases.contains(sensorId);
    };
    if (!calibrated(*proximal) || !calibrated(*distal)) {
        return std::nullopt;
    }

    return FunctionalSensorPair{*proximal, *distal};
}

FunctionalCalibrationCaptureWorkflow::FunctionalCalibrationCaptureWorkflow(
    frontend_protocol::EventWriter& frontend, std::ostream& output, std::ostream& errorOutput,
    const std::chrono::seconds preparationSeconds, const std::chrono::seconds gravityCaptureSeconds,
    const std::chrono::seconds movementCaptureSeconds)
    : frontend_{frontend}, output_{output}, errorOutput_{errorOutput},
      preparationSeconds_{preparationSeconds}, gravityCaptureSeconds_{gravityCaptureSeconds},
      movementCaptureSeconds_{movementCaptureSeconds} {}

std::optional<FunctionalCalibrationCapture> FunctionalCalibrationCaptureWorkflow::capture(
    const calibration::JointCalibrationDefinition& joint, const SensorMapping& sensorMapping,
    const CalibrationSession& calibrationSession, PacketCollector& packetCollector) const {

    const auto sensors = resolveFunctionalSensorPair(
        joint, sensorMapping.sensorToSegment(), calibrationSession.staticOffsets(),
        calibrationSession.finalOffsets(), calibrationSession.gyroBiases());
    if (!sensors) {
        frontend_.emitError(frontend_protocol::error::kFunctionalCalibration,
                            joint.displayName + ":Required calibrated sensors unavailable");
        errorOutput_ << "Required calibrated sensors are unavailable." << std::endl;
        return std::nullopt;
    }

    frontend_.emitGuidance(frontend_protocol::guidance::kFunctionalGravityPreparation,
                           joint.displayName);
    output_ << "\n--- PHASE 1: STATIC (Gravity) ---" << std::endl;
    output_ << "Return both segments to the same pose used for static "
            << "calibration and hold still." << std::endl;
    output_ << "Starting in " << preparationSeconds_.count() << " seconds..." << std::endl;
    frontend_.runGuidedCountdown("FUNCTIONAL_GRAVITY",
                                 static_cast<int>(preparationSeconds_.count()));

    frontend_.emitGuidance(frontend_protocol::guidance::kFunctionalGravityCapture,
                           joint.displayName + "|" +
                               std::to_string(gravityCaptureSeconds_.count()));
    output_ << "RECORDING STATIC ACCEL DATA... DO NOT MOVE." << std::endl;
    packetCollector.clearCalibrationBuffers();
    std::this_thread::sleep_for(gravityCaptureSeconds_);
    auto gravityPackets = packetCollector.takeCalibrationPackets();
    frontend_.emitGuidance(frontend_protocol::guidance::kFunctionalGravityProcessing,
                           joint.displayName);

    const auto proximalGravityPackets = gravityPackets.find(sensors->proximalSensor);
    const auto distalGravityPackets = gravityPackets.find(sensors->distalSensor);
    if (proximalGravityPackets == gravityPackets.end() ||
        distalGravityPackets == gravityPackets.end()) {

        frontend_.emitError(frontend_protocol::error::kFunctionalCalibration,
                            joint.displayName + ":Missing gravity samples");
        errorOutput_ << "Missing gravity samples for one or both sensors." << std::endl;
        return std::nullopt;
    }

    const GravityDirectionEstimate proximalGravity = SeatedCalibration::computeGravityDirection(
        calibration_analysis::extractAccelerations(proximalGravityPackets->second));

    const GravityDirectionEstimate distalGravity = SeatedCalibration::computeGravityDirection(
        calibration_analysis::extractAccelerations(distalGravityPackets->second));

    if (!proximalGravity.valid || !distalGravity.valid) {
        frontend_.emitError(frontend_protocol::error::kFunctionalCalibration,
                            joint.displayName + ":Static gravity capture rejected");
        errorOutput_ << "Static gravity capture was rejected.\n"
                     << "Proximal: " << proximalGravity.message << "\n"
                     << "Distal: " << distalGravity.message << std::endl;
        return std::nullopt;
    }

    const double radiansToDegrees = 180.0 / std::acos(-1.0);

    output_ << "Gravity uncertainty diagnostics for " << joint.displayName << "\n"
            << "  Proximal valid blocks: " << proximalGravity.usedBlocks << "\n"
            << "  Proximal angular sigma: "
            << proximalGravity.angularUncertaintyRadians * radiansToDegrees << " degrees\n"
            << "  Distal valid blocks: " << distalGravity.usedBlocks << "\n"
            << "  Distal angular sigma: "
            << distalGravity.angularUncertaintyRadians * radiansToDegrees << " degrees"
            << std::endl;

    frontend_.emitGuidance(frontend_protocol::guidance::kFunctionalMovementPreparation,
                           joint.displayName);
    output_ << "\n--- PHASE 2: DYNAMIC FUNCTIONAL MOVEMENT ---" << std::endl;
    if (joint.movementType == calibration::FunctionalMovementType::PronationSupination) {

        output_ << "Keep the upper arm beside the torso and the elbow "
                << "at approximately 90 degrees.\n"
                << "Keep the wrist neutral.\n"
                << "Begin with SUPINATION, then repeatedly turn the "
                << "palm up and down through a comfortable range.\n"
                << "Avoid moving the shoulder, elbow or wrist." << std::endl;
    } else if (joint.movementType ==
               calibration::FunctionalMovementType::ShoulderAbductionAdduction) {

        output_ << "Keep the elbow at a fixed comfortable angle.\n"
                << "Keep the forearm and palm orientation fixed.\n"
                << "Begin with ABDUCTION, raising the arm sideways, then "
                << "repeatedly lower and raise it through a comfortable range.\n"
                << "Avoid shrugging, torso leaning and upper-arm "
                << "internal/external rotation." << std::endl;
    } else {
        output_ << "Move " << joint.displayName
                << " beginning with flexion, then repeatedly move through "
                << "comfortable flexion/extension.\n"
                << "Avoid rotation outside the hinge plane." << std::endl;
    }
    output_ << "Starting in " << preparationSeconds_.count() << " seconds..." << std::endl;
    frontend_.runGuidedCountdown("FUNCTIONAL_MOVEMENT",
                                 static_cast<int>(preparationSeconds_.count()));

    frontend_.emitGuidance(frontend_protocol::guidance::kFunctionalMovementCapture,
                           joint.displayName + "|" +
                               std::to_string(movementCaptureSeconds_.count()));
    packetCollector.clearCalibrationBuffers();
    output_ << "RECORDING SYNCHRONIZED GYRO DATA FOR " << movementCaptureSeconds_.count()
            << " SECONDS..." << std::endl;
    std::this_thread::sleep_for(movementCaptureSeconds_);
    auto dynamicPackets = packetCollector.takeCalibrationPackets();
    frontend_.emitGuidance(frontend_protocol::guidance::kFunctionalProcessing, joint.displayName);

    if (!dynamicPackets.contains(sensors->proximalSensor) ||
        !dynamicPackets.contains(sensors->distalSensor)) {

        frontend_.emitError(frontend_protocol::error::kFunctionalCalibration,
                            joint.displayName + ":Missing dynamic samples");
        errorOutput_ << "Missing dynamic samples for one or both sensors." << std::endl;
        return std::nullopt;
    }

    return FunctionalCalibrationCapture{*sensors, proximalGravity, distalGravity,
                                        std::move(gravityPackets), std::move(dynamicPackets)};
}
