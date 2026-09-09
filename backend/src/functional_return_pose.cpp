#include "functional_return_pose.h"

#include "calibration_session.h"
#include "frontend_protocol.h"
#include "packet_collector.h"

#include <ostream>
#include <thread>
#include <utility>

namespace {

constexpr double MAXIMUM_STATIC_REFERENCE_ERROR_DEGREES = 20.0;
constexpr double MAXIMUM_CANDIDATE_ERROR_DEGREES = 40.0;

} // namespace

ReturnPoseDecision
evaluateReturnPose(const calibration_analysis::ReturnPoseValidationResult& proximal,
                   const calibration_analysis::ReturnPoseValidationResult& distal,
                   const double maximumStaticReferenceErrorDegrees,
                   const double maximumCandidateErrorDegrees) noexcept {

    if (!proximal.valid || !distal.valid) {
        return ReturnPoseDecision::InvalidCapture;
    }
    if (proximal.staticReferenceErrorDegrees > maximumStaticReferenceErrorDegrees ||
        distal.staticReferenceErrorDegrees > maximumStaticReferenceErrorDegrees) {

        return ReturnPoseDecision::ReferencePoseNotReproduced;
    }
    if (proximal.candidateErrorDegrees > maximumCandidateErrorDegrees ||
        distal.candidateErrorDegrees > maximumCandidateErrorDegrees) {
        return ReturnPoseDecision::CandidateErrorTooLarge;
    }
    return ReturnPoseDecision::Accept;
}

FunctionalReturnPoseWorkflow::FunctionalReturnPoseWorkflow(
    frontend_protocol::EventWriter& frontend, std::ostream& output, std::ostream& errorOutput,
    const std::chrono::seconds preparationSeconds, const std::chrono::seconds captureSeconds)
    : frontend_{frontend}, output_{output}, errorOutput_{errorOutput},
      preparationSeconds_{preparationSeconds}, captureSeconds_{captureSeconds} {}

std::optional<FunctionalReturnPoseResult> FunctionalReturnPoseWorkflow::captureValidateAndCommit(
    const calibration::JointCalibrationDefinition& joint,
    const FunctionalCalibrationCapture& functionalCapture, FunctionalRefinementCandidate candidate,
    CalibrationSession& calibrationSession, PacketCollector& packetCollector) const {

    frontend_.emitGuidance(frontend_protocol::guidance::kFunctionalReturnPosePreparation,
                           joint.displayName);
    output_ << "\n--- PHASE 3: RETURN TO STATIC POSE ---\n"
            << "Return both segments to the original static calibration pose.\n"
            << "Hold the pose still during the validation capture." << std::endl;
    frontend_.runGuidedCountdown("FUNCTIONAL_RETURN_POSE",
                                 static_cast<int>(preparationSeconds_.count()));

    packetCollector.clearCalibrationBuffers();
    frontend_.emitGuidance(frontend_protocol::guidance::kFunctionalReturnPoseCapture,
                           joint.displayName + "|" + std::to_string(captureSeconds_.count()));
    output_ << "RECORDING RETURN-POSE DATA FOR " << captureSeconds_.count()
            << " SECONDS... DO NOT MOVE." << std::endl;
    std::this_thread::sleep_for(captureSeconds_);
    auto packets = packetCollector.takeCalibrationPackets();

    const std::string& proximalSensor = functionalCapture.sensors.proximalSensor;
    const std::string& distalSensor = functionalCapture.sensors.distalSensor;
    const auto proximalPackets = packets.find(proximalSensor);
    const auto distalPackets = packets.find(distalSensor);
    if (proximalPackets == packets.end() || distalPackets == packets.end()) {
        frontend_.emitError(frontend_protocol::error::kFunctionalCalibration,
                            joint.displayName + ":Missing return-pose packets");
        errorOutput_ << "Independent calibration rejected: missing return-pose "
                     << "packets for one or both sensors." << std::endl;
        return std::nullopt;
    }

    FunctionalReturnPoseResult result;
    result.proximalRefinement = candidate.proximalResult;
    result.distalRefinement = candidate.distalResult;
    result.proximalValidation = calibration_analysis::validateReturnToStaticPose(
        proximalPackets->second, calibrationSession.activePoseTargets().at(joint.proximalSegment),
        result.proximalRefinement.offset, calibrationSession.staticOffsets().at(proximalSensor),
        calibrationSession.globalToSessionRotation());
    result.distalValidation = calibration_analysis::validateReturnToStaticPose(
        distalPackets->second, calibrationSession.activePoseTargets().at(joint.distalSegment),
        result.distalRefinement.offset, calibrationSession.staticOffsets().at(distalSensor),
        calibrationSession.globalToSessionRotation());

    const ReturnPoseDecision decision =
        evaluateReturnPose(result.proximalValidation, result.distalValidation,
                           MAXIMUM_STATIC_REFERENCE_ERROR_DEGREES, MAXIMUM_CANDIDATE_ERROR_DEGREES);
    if (decision == ReturnPoseDecision::InvalidCapture) {
        frontend_.emitError(frontend_protocol::error::kFunctionalCalibration,
                            joint.displayName + ":Return-pose capture rejected");
        errorOutput_ << "Return-to-static-pose validation rejected.\n"
                     << "Proximal: " << result.proximalValidation.message << "\n"
                     << "Distal: " << result.distalValidation.message << std::endl;
        return std::nullopt;
    }
    if (decision == ReturnPoseDecision::ReferencePoseNotReproduced) {
        frontend_.emitError(frontend_protocol::error::kFunctionalCalibration,
                            joint.displayName + ":Reference static pose was not reproduced");
        errorOutput_ << "Independent calibration not evaluated because the original "
                     << "static pose was not reproduced closely enough.\n"
                     << "Proximal static-reference error: "
                     << result.proximalValidation.staticReferenceErrorDegrees
                     << " degrees\nDistal static-reference error: "
                     << result.distalValidation.staticReferenceErrorDegrees
                     << " degrees\nMaximum allowed: " << MAXIMUM_STATIC_REFERENCE_ERROR_DEGREES
                     << " degrees" << std::endl;
        return std::nullopt;
    }
    if (decision == ReturnPoseDecision::CandidateErrorTooLarge) {
        frontend_.emitError(frontend_protocol::error::kFunctionalCalibration,
                            joint.displayName + ":Independent return-pose error too large");
        errorOutput_ << "Functional calibration rejected by return-pose validation.\n"
                     << "Proximal static-reference error: "
                     << result.proximalValidation.staticReferenceErrorDegrees << " degrees\n"
                     << "Proximal functional-candidate error: "
                     << result.proximalValidation.candidateErrorDegrees << " degrees\n"
                     << "Proximal deterioration: "
                     << result.proximalValidation.candidateErrorDegrees -
                            result.proximalValidation.staticReferenceErrorDegrees
                     << " degrees\n"
                     << "Distal static-reference error: "
                     << result.distalValidation.staticReferenceErrorDegrees << " degrees\n"
                     << "Distal functional-candidate error: "
                     << result.distalValidation.candidateErrorDegrees << " degrees\n"
                     << "Distal deterioration: "
                     << result.distalValidation.candidateErrorDegrees -
                            result.distalValidation.staticReferenceErrorDegrees
                     << " degrees" << std::endl;
        return std::nullopt;
    }

    const bool refinesDistalOnly =
        joint.movementType != calibration::FunctionalMovementType::FlexionExtension;

    if (refinesDistalOnly) {
        calibrationSession.commitSingleFunctionalCalibration(
            std::move(candidate.observations), distalSensor, result.distalRefinement.offset);
    } else {
        calibrationSession.commitFunctionalCalibration(
            std::move(candidate.observations), proximalSensor, result.proximalRefinement.offset,
            distalSensor, result.distalRefinement.offset);
    }

    result.packets = std::move(packets);
    output_ << "\nReturn-to-static-pose validation passed for " << joint.displayName << "\n"
            << "Proximal sensor:\n  Stationary samples: "
            << result.proximalValidation.stationarySamples << "\n  Static-reference error: "
            << result.proximalValidation.staticReferenceErrorDegrees
            << " degrees\n  Independent candidate error: "
            << result.proximalValidation.candidateErrorDegrees
            << " degrees\nDistal sensor:\n  Stationary samples: "
            << result.distalValidation.stationarySamples
            << "\n  Static-reference error: " << result.distalValidation.staticReferenceErrorDegrees
            << " degrees\n  Independent candidate error: "
            << result.distalValidation.candidateErrorDegrees << " degrees" << std::endl;

    return result;
}
