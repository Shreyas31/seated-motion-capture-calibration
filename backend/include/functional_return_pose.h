#pragma once

#include "calibration_sample_analysis.h"
#include "functional_calibration_capture.h"
#include "functional_calibration_solver.h"

#include <chrono>
#include <iosfwd>
#include <map>
#include <optional>
#include <string>
#include <vector>

class CalibrationSession;
class PacketCollector;
namespace frontend_protocol {
class EventWriter;
}

/**
 * Classifies the outcome of paired return-to-reference-pose validation.
 * @enum ReturnPoseDecision.
 */
enum class ReturnPoseDecision {
    Accept,
    InvalidCapture,
    ReferencePoseNotReproduced,
    CandidateErrorTooLarge
};

/**
 * Applies ordered acceptance thresholds to paired return-pose validation results.
 * @param proximal Validation result for the proximal sensor.
 * @param distal Validation result for the distal sensor.
 * @param maximumStaticReferenceErrorDegrees Maximum allowed original-static pose error.
 * @param maximumCandidateErrorDegrees Maximum allowed candidate-offset pose error.
 * @return Decision explaining whether and why the candidate should be committed.
 */
ReturnPoseDecision
evaluateReturnPose(const calibration_analysis::ReturnPoseValidationResult& proximal,
                   const calibration_analysis::ReturnPoseValidationResult& distal,
                   double maximumStaticReferenceErrorDegrees = 20.0,
                   double maximumCandidateErrorDegrees = 20.0) noexcept;

/**
 * Stores captured return-pose evidence and the final paired refinement results.
 * @struct FunctionalReturnPoseResult.
 */
struct FunctionalReturnPoseResult {
    std::map<std::string, std::vector<XsDataPacket>> packets;
    calibration_analysis::ReturnPoseValidationResult proximalValidation;
    calibration_analysis::ReturnPoseValidationResult distalValidation;
    FunctionalRefinementResult proximalRefinement;
    FunctionalRefinementResult distalRefinement;
};

/**
 * Captures a return pose, validates candidate offsets, and commits accepted calibration.
 * @class FunctionalReturnPoseWorkflow.
 */
class FunctionalReturnPoseWorkflow {
  public:
    /**
     * Creates a timed return-pose validation stage.
     * @param frontend Writer for structured guidance, errors, and results.
     * @param output Stream used for clinician instructions.
     * @param errorOutput Stream used for validation diagnostics.
     * @param preparationSeconds Countdown before return-pose capture.
     * @param captureSeconds Duration of the stationary return-pose capture.
     */
    FunctionalReturnPoseWorkflow(frontend_protocol::EventWriter& frontend, std::ostream& output,
                                 std::ostream& errorOutput,
                                 std::chrono::seconds preparationSeconds = std::chrono::seconds{5},
                                 std::chrono::seconds captureSeconds = std::chrono::seconds{5});

    /**
     * Validates a proposed paired refinement against a reproduced static reference pose.
     * @param joint Selected functional joint definition.
     * @param functionalCapture Original functional capture and sensor IDs.
     * @param candidate Proposed offsets and accumulated vector observations.
     * @param calibrationSession Session committed only after all validation checks pass.
     * @param packetCollector Collector used for the timed stationary return-pose capture.
     * @return Validation evidence and committed refinements, or no value when rejected.
     */
    std::optional<FunctionalReturnPoseResult>
    captureValidateAndCommit(const calibration::JointCalibrationDefinition& joint,
                             const FunctionalCalibrationCapture& functionalCapture,
                             FunctionalRefinementCandidate candidate,
                             CalibrationSession& calibrationSession,
                             PacketCollector& packetCollector) const;

  private:
    frontend_protocol::EventWriter& frontend_;
    std::ostream& output_;
    std::ostream& errorOutput_;
    std::chrono::seconds preparationSeconds_;
    std::chrono::seconds captureSeconds_;
};
