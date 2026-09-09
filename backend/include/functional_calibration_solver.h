#pragma once

#include "calibration_sample_analysis.h"
#include "functional_calibration_capture.h"
#include "seated_calibration.h"
#include "seated_pose_targets.h"

#include <Eigen>
#include <iosfwd>
#include <map>
#include <optional>
#include <string>
#include <vector>

class CalibrationSession;
namespace frontend_protocol {
class EventWriter;
}

/**
 * Stores expected anatomical movement axes for the proximal and distal segments.
 * @struct FunctionalTargetAxes.
 */
struct FunctionalTargetAxes {
    Eigen::Vector3d proximal;
    Eigen::Vector3d distal;
};

/**
 * Combines synchronised samples, estimated axes, and anatomical targets for refinement.
 * @struct FunctionalAxisAnalysis.
 */
struct FunctionalAxisAnalysis {
    calibration_analysis::RelativeAxisSamples relativeSamples;
    FunctionalAxisEstimate proximalAxis;
    FunctionalAxisEstimate distalAxis;
    FunctionalTargetAxes targetAxes;
    bool isPronationSupination = false;
    bool isShoulderAbductionAdduction = false;

    // Elbow and knee flexion refine both sensors. Pronation and shoulder
    // abduction contribute only to the moving distal segment.
    bool refinesProximalSegment = true;
};

/**
 * Builds anatomical target axes for the selected movement.
 * @param joint Functional movement definition.
 * @return Proximal and distal unit axes expressed in their anatomical frames B.
 */
FunctionalTargetAxes
buildFunctionalTargetAxes(const calibration::JointCalibrationDefinition& joint);

/**
 * Estimates paired functional axes and rejects inadequate movement captures.
 * @class FunctionalAxisWorkflow.
 */
class FunctionalAxisWorkflow {
  public:
    /**
     * Creates an axis-analysis stage using injected reporting streams.
     * @param frontend Writer for structured calibration results and errors.
     * @param output Stream used for axis-quality diagnostics.
     * @param errorOutput Stream used for rejected-analysis messages.
     */
    FunctionalAxisWorkflow(frontend_protocol::EventWriter& frontend, std::ostream& output,
                           std::ostream& errorOutput);

    /**
     * Synchronizes samples, removes biases, estimates axes, and checks confidence.
     * @param joint Selected functional joint definition.
     * @param capture Gravity and movement data for the resolved sensor pair.
     * @param calibrationSession Session providing gyro biases and palm orientation.
     * @return Complete axis analysis, or no value if samples or estimates fail quality checks.
     */
    std::optional<FunctionalAxisAnalysis>
    analyze(const calibration::JointCalibrationDefinition& joint,
            const FunctionalCalibrationCapture& capture,
            const CalibrationSession& calibrationSession) const;

  private:
    frontend_protocol::EventWriter& frontend_;
    std::ostream& output_;
    std::ostream& errorOutput_;
};

/**
 * Stores proposed paired offsets and the observation history used to calculate them.
 * @struct FunctionalRefinementCandidate.
 */
struct FunctionalRefinementCandidate {
    std::map<std::string, std::vector<CalibrationVectorObservation>> observations;
    FunctionalRefinementResult proximalResult;
    FunctionalRefinementResult distalResult;
};

/**
 * Replaces an observation with the same source label or appends a new source.
 * @param observations Mutable observation list for one sensor.
 * @param replacement Observation whose source identifies the entry to replace.
 */
void replaceObservationBySource(std::vector<CalibrationVectorObservation>& observations,
                                const CalibrationVectorObservation& replacement);

/**
 * Adds an observation only when its source is not already present.
 * This retains the first accepted session gravity observation.
 * @param observations Mutable observation list for one sensor.
 * @param observation Observation to add if its source is unique.
 */
void addObservationIfSourceMissing(std::vector<CalibrationVectorObservation>& observations,
                                   const CalibrationVectorObservation& observation);
/**
 * Computes separation between two unoriented axes.
 * @param first First axis vector.
 * @param second Second axis vector.
 * @return Smallest line-to-line angular separation in degrees in the range [0, 90].
 */
double unsignedAxisSeparationDegrees(const Eigen::Vector3d& first, const Eigen::Vector3d& second);

/**
 * Builds and solves functional-refinement observations for both joint sensors.
 * @class FunctionalRefinementWorkflow.
 */
class FunctionalRefinementWorkflow {
  public:
    /**
     * Creates a refinement stage using injected reporting channels.
     * @param frontend Writer for structured refinement errors.
     * @param errorOutput Stream used for numerical-quality diagnostics.
     */
    FunctionalRefinementWorkflow(frontend_protocol::EventWriter& frontend,
                                 std::ostream& errorOutput);

    /**
     * Solves updated offsets without mutating the calibration session.
     * @param joint Selected functional joint definition.
     * @param capture Captured gravity vectors and resolved sensor IDs.
     * @param axisAnalysis Estimated functional axes and anatomical targets.
     * @param calibrationSession Session providing static offsets and earlier observations.
     * @return Candidate offsets and observations, or no value when refinement is rejected.
     */
    std::optional<FunctionalRefinementCandidate>
    solve(const calibration::JointCalibrationDefinition& joint,
          const FunctionalCalibrationCapture& capture, const FunctionalAxisAnalysis& axisAnalysis,
          const CalibrationSession& calibrationSession) const;

  private:
    frontend_protocol::EventWriter& frontend_;
    std::ostream& errorOutput_;
};
