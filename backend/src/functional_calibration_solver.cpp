#include "functional_calibration_solver.h"

#include "calibration_session.h"
#include "frontend_protocol.h"
#include "seated_pose_targets.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <ostream>
#include <utility>

FunctionalTargetAxes
buildFunctionalTargetAxes(const calibration::JointCalibrationDefinition& joint) {
    FunctionalTargetAxes targets{joint.proximalAxisB, joint.distalAxisB};

    switch (joint.movementType) {
    case calibration::FunctionalMovementType::PronationSupination:

        // Forearm rotation uses the longitudinal anatomical axis.
        targets.distal = Eigen::Vector3d::UnitY();
        break;

    case calibration::FunctionalMovementType::ShoulderAbductionAdduction:

        // Shoulder abduction uses the upper-arm anterior-posterior axis.
        targets.distal = Eigen::Vector3d::UnitX();
        break;

    case calibration::FunctionalMovementType::FlexionExtension:

        // The palms-together forearm frame has a side-specific elbow axis.
        if (joint.distalSegment == "Right_Forearm") {
            targets.distal = buildForearmElbowAxisTarget(true);
        } else if (joint.distalSegment == "Left_Forearm") {
            targets.distal = buildForearmElbowAxisTarget(false);
        }
        break;
    }

    return targets;
}

FunctionalAxisWorkflow::FunctionalAxisWorkflow(frontend_protocol::EventWriter& frontend,
                                               std::ostream& output, std::ostream& errorOutput)
    : frontend_{frontend}, output_{output}, errorOutput_{errorOutput} {}

std::optional<FunctionalAxisAnalysis>
FunctionalAxisWorkflow::analyze(const calibration::JointCalibrationDefinition& joint,
                                const FunctionalCalibrationCapture& capture,
                                const CalibrationSession& calibrationSession) const {

    const auto& proximalPackets = capture.dynamicPackets.at(capture.sensors.proximalSensor);

    const auto& distalPackets = capture.dynamicPackets.at(capture.sensors.distalSensor);

    FunctionalAxisAnalysis analysis;

    analysis.isPronationSupination =
        joint.movementType == calibration::FunctionalMovementType::PronationSupination;

    analysis.isShoulderAbductionAdduction =
        joint.movementType == calibration::FunctionalMovementType::ShoulderAbductionAdduction;

    analysis.refinesProximalSegment =
        joint.movementType == calibration::FunctionalMovementType::FlexionExtension;

    analysis.targetAxes = buildFunctionalTargetAxes(joint);

    // Relative angular velocity requires both sensors, even for distal-only refinement.
    analysis.relativeSamples = calibration_analysis::buildRelativeAxisSamples(
        proximalPackets, distalPackets,
        calibrationSession.gyroBiases().at(capture.sensors.proximalSensor),
        calibrationSession.gyroBiases().at(capture.sensors.distalSensor));

    analysis.distalAxis =
        SeatedCalibration::computeFunctionalAxisWithUncertainty(analysis.relativeSamples.distal);

    if (analysis.refinesProximalSegment) {
        analysis.proximalAxis = SeatedCalibration::computeFunctionalAxisWithUncertainty(
            analysis.relativeSamples.proximal);
    }

    if (!analysis.distalAxis.valid ||
        (analysis.refinesProximalSegment && !analysis.proximalAxis.valid)) {

        frontend_.emitError(frontend_protocol::error::kFunctionalCalibration,
                            joint.displayName + ":Functional axis estimate rejected");

        errorOutput_ << "Functional capture rejected for " << joint.displayName << "\n";

        if (analysis.refinesProximalSegment) {
            errorOutput_ << "Proximal: " << analysis.proximalAxis.message << "\n"
                         << "  Axis S: " << analysis.proximalAxis.axis.transpose() << "\n"
                         << "  Confidence: " << analysis.proximalAxis.confidence << "\n"
                         << "  RMS angular speed: " << analysis.proximalAxis.rmsAngularSpeed
                         << " rad/s\n"
                         << "  Used samples: " << analysis.proximalAxis.usedSamples << "\n";

            constexpr double RADIANS_TO_DEGREES = 180.0 / 3.14159265358979323846;

            if (std::isfinite(analysis.proximalAxis.angularUncertaintyRadians)) {
                std::cout << "Proximal functional-axis uncertainty: "
                          << analysis.proximalAxis.angularUncertaintyRadians * RADIANS_TO_DEGREES
                          << " degrees\n"
                          << "Proximal uncertainty blocks: "
                          << analysis.proximalAxis.uncertaintyBlocks << '\n';
            }
        }

        errorOutput_ << "Distal: " << analysis.distalAxis.message << "\n"
                     << "  Axis S: " << analysis.distalAxis.axis.transpose() << "\n"
                     << "  Confidence: " << analysis.distalAxis.confidence << "\n"
                     << "  RMS angular speed: " << analysis.distalAxis.rmsAngularSpeed << " rad/s\n"
                     << "  Used samples: " << analysis.distalAxis.usedSamples << std::endl;

        constexpr double RADIANS_TO_DEGREES = 180.0 / 3.14159265358979323846;

        if (std::isfinite(analysis.distalAxis.angularUncertaintyRadians)) {
            std::cout << "Distal functional-axis uncertainty: "
                      << analysis.distalAxis.angularUncertaintyRadians * RADIANS_TO_DEGREES
                      << " degrees\n"
                      << "Distal uncertainty blocks: " << analysis.distalAxis.uncertaintyBlocks
                      << '\n';
        }

        return std::nullopt;
    }

    output_ << "\nPre-Wahba diagnostics for " << joint.displayName << "\n";

    if (analysis.refinesProximalSegment) {
        output_ << "Proximal sensor axis S: " << analysis.proximalAxis.axis.transpose() << "\n"
                << "Expected proximal axis B: " << analysis.targetAxes.proximal.transpose() << "\n"
                << "Proximal PCA confidence: " << analysis.proximalAxis.confidence << "\n"
                << "Proximal RMS angular speed: " << analysis.proximalAxis.rmsAngularSpeed
                << " rad/s\n"
                << "Proximal speed-qualified samples: " << analysis.proximalAxis.candidateSamples
                << "\n"
                << "Proximal rejected outliers: " << analysis.proximalAxis.rejectedOutliers << "\n"
                << "Proximal inlier fraction: " << analysis.proximalAxis.inlierFraction << "\n";
    }

    output_ << "Distal sensor axis S: " << analysis.distalAxis.axis.transpose() << "\n"
            << "Expected distal axis B: " << analysis.targetAxes.distal.transpose() << "\n"
            << "Distal PCA confidence: " << analysis.distalAxis.confidence << "\n"
            << "Distal RMS angular speed: " << analysis.distalAxis.rmsAngularSpeed << " rad/s\n"
            << "Distal speed-qualified samples: " << analysis.distalAxis.candidateSamples << "\n"
            << "Distal rejected outliers: " << analysis.distalAxis.rejectedOutliers << "\n"
            << "Distal inlier fraction: " << analysis.distalAxis.inlierFraction << "\n"
            << "Synchronized packets: " << analysis.relativeSamples.synchronizedPackets
            << std::endl;

    return analysis;
}

namespace {

constexpr double PI = 3.14159265358979323846;
constexpr double GRAVITY_WEIGHT = 1.0;

// MAD measures repeatability rather than absolute accuracy; enforce a realistic floor.
constexpr double MINIMUM_GRAVITY_UNCERTAINTY_RADIANS = 1.0 * PI / 180.0;
constexpr double MINIMUM_FUNCTIONAL_UNCERTAINTY_RADIANS = 1.0 * PI / 180.0;

// Keep a short capture from dominating or becoming irrelevant.
constexpr double MINIMUM_FUNCTIONAL_WEIGHT = 0.25;
constexpr double MAXIMUM_FUNCTIONAL_WEIGHT = 4.0;

double effectiveGravityUncertainty(double measured_uncertainty_radians,
                                   double session_floor_radians) {

    return (std::max)((std::max)(measured_uncertainty_radians, session_floor_radians),
                      MINIMUM_GRAVITY_UNCERTAINTY_RADIANS);
}

double effectiveFunctionalUncertainty(double measured_uncertainty_radians) {

    return (std::max)(measured_uncertainty_radians, MINIMUM_FUNCTIONAL_UNCERTAINTY_RADIANS);
}

double functionalWeightRelativeToGravity(double gravity_uncertainty_radians,
                                         double gravity_uncertainty_floor_radians,
                                         double functional_uncertainty_radians) {

    if (!std::isfinite(gravity_uncertainty_radians) ||
        !std::isfinite(gravity_uncertainty_floor_radians) ||
        !std::isfinite(functional_uncertainty_radians) || gravity_uncertainty_radians < 0.0 ||
        gravity_uncertainty_floor_radians < 0.0 || functional_uncertainty_radians < 0.0) {

        return 0.0;
    }

    const double effective_gravity_uncertainty =
        effectiveGravityUncertainty(gravity_uncertainty_radians, gravity_uncertainty_floor_radians);

    const double effective_functional_uncertainty =
        effectiveFunctionalUncertainty(functional_uncertainty_radians);

    // Inverse-variance weighting relative to gravity: sigma_g^2 / sigma_f^2.
    const double ratio = effective_gravity_uncertainty / effective_functional_uncertainty;

    const double raw_weight = ratio * ratio;

    if (!std::isfinite(raw_weight) || raw_weight <= 0.0) {
        return 0.0;
    }

    return std::clamp(raw_weight, MINIMUM_FUNCTIONAL_WEIGHT, MAXIMUM_FUNCTIONAL_WEIGHT);
}

double rawFunctionalWeightRelativeToGravity(double gravity_uncertainty_radians,
                                            double gravity_uncertainty_floor_radians,
                                            double functional_uncertainty_radians) {

    if (!std::isfinite(gravity_uncertainty_radians) ||
        !std::isfinite(gravity_uncertainty_floor_radians) ||
        !std::isfinite(functional_uncertainty_radians) || gravity_uncertainty_radians < 0.0 ||
        gravity_uncertainty_floor_radians < 0.0 || functional_uncertainty_radians < 0.0) {

        return 0.0;
    }

    const double effective_gravity_uncertainty =
        effectiveGravityUncertainty(gravity_uncertainty_radians, gravity_uncertainty_floor_radians);

    const double effective_functional_uncertainty =
        effectiveFunctionalUncertainty(functional_uncertainty_radians);

    const double ratio = effective_gravity_uncertainty / effective_functional_uncertainty;

    return ratio * ratio;
}

constexpr double MINIMUM_AXIS_SEPARATION_DEGREES = 20.0;
constexpr double MAXIMUM_RESIDUAL_DEGREES = 20.0;

} // namespace

void replaceObservationBySource(std::vector<CalibrationVectorObservation>& observations,
                                const CalibrationVectorObservation& replacement) {

    observations.erase(std::remove_if(observations.begin(), observations.end(),
                                      [&replacement](const CalibrationVectorObservation& existing) {
                                          return existing.source == replacement.source;
                                      }),
                       observations.end());
    observations.push_back(replacement);
}

void addObservationIfSourceMissing(std::vector<CalibrationVectorObservation>& observations,
                                   const CalibrationVectorObservation& observation) {
    const auto existing =
        std::find_if(observations.begin(), observations.end(),
                     [&observation](const CalibrationVectorObservation& candidate) {
                         return candidate.source == observation.source;
                     });

    if (existing == observations.end()) {
        observations.push_back(observation);
    }
}

double unsignedAxisSeparationDegrees(const Eigen::Vector3d& first, const Eigen::Vector3d& second) {

    const double cosine =
        std::clamp(std::abs(first.normalized().dot(second.normalized())), 0.0, 1.0);
    return std::acos(cosine) * 180.0 / std::acos(-1.0);
}

FunctionalRefinementWorkflow::FunctionalRefinementWorkflow(frontend_protocol::EventWriter& frontend,
                                                           std::ostream& errorOutput)
    : frontend_{frontend}, errorOutput_{errorOutput} {}

std::optional<FunctionalRefinementCandidate>
FunctionalRefinementWorkflow::solve(const calibration::JointCalibrationDefinition& joint,
                                    const FunctionalCalibrationCapture& capture,
                                    const FunctionalAxisAnalysis& axisAnalysis,
                                    const CalibrationSession& calibrationSession) const {

    const std::string& proximalSensor = capture.sensors.proximalSensor;
    const std::string& distalSensor = capture.sensors.distalSensor;
    const Eigen::Vector3d proximalTargetGravity =
        calibrationSession.activePoseTargets().at(joint.proximalSegment).conjugate() *
        Eigen::Vector3d::UnitZ();
    const Eigen::Vector3d distalTargetGravity =
        calibrationSession.activePoseTargets().at(joint.distalSegment).conjugate() *
        Eigen::Vector3d::UnitZ();

    auto observations = calibrationSession.calibrationObservations();
    replaceObservationBySource(
        observations[distalSensor],
        CalibrationVectorObservation{
            capture.distalGravity.direction, distalTargetGravity, GRAVITY_WEIGHT, true,
            "Static-pose gravity",
            effectiveGravityUncertainty(capture.distalGravity.angularUncertaintyRadians,
                                        calibrationSession.gravityUncertaintyFloorRadians())});

    std::string distalFunctionalSource;

    if (axisAnalysis.isPronationSupination) {
        distalFunctionalSource = "Forearm pronation/supination axis";
    } else if (axisAnalysis.isShoulderAbductionAdduction) {
        distalFunctionalSource = joint.displayName + " axis";
    } else {
        distalFunctionalSource = joint.displayName + " flexion/extension axis";
    }

    const double distalFunctionalWeight =
        functionalWeightRelativeToGravity(capture.distalGravity.angularUncertaintyRadians,
                                          calibrationSession.gravityUncertaintyFloorRadians(),
                                          axisAnalysis.distalAxis.angularUncertaintyRadians);
    const double distalRawFunctionalWeight =
        rawFunctionalWeightRelativeToGravity(capture.distalGravity.angularUncertaintyRadians,
                                             calibrationSession.gravityUncertaintyFloorRadians(),
                                             axisAnalysis.distalAxis.angularUncertaintyRadians);
    if (!(distalFunctionalWeight > 0.0) || !std::isfinite(distalFunctionalWeight)) {

        frontend_.emitError(frontend_protocol::error::kFunctionalCalibration,
                            joint.displayName +
                                ":Could not calculate a valid distal observation weight");

        errorOutput_ << "Functional calibration rejected: invalid distal "
                     << "uncertainty-based weight.\n"
                     << "  Gravity uncertainty: " << capture.distalGravity.angularUncertaintyRadians
                     << " rad\n"
                     << "  Gravity floor: " << calibrationSession.gravityUncertaintyFloorRadians()
                     << " rad\n"
                     << "  Functional-axis uncertainty: "
                     << axisAnalysis.distalAxis.angularUncertaintyRadians << " rad" << std::endl;

        return std::nullopt;
    }

    const Eigen::Quaterniond& distalStaticOffset =
        calibrationSession.staticOffsets().at(distalSensor);

    const Eigen::Vector3d distalAxisMappedByStatic =
        distalStaticOffset * axisAnalysis.distalAxis.axis;

    const Eigen::Vector3d distalGravityMappedByStatic =
        distalStaticOffset * capture.distalGravity.direction;

    errorOutput_ << "Distal frame-consistency diagnostics:\n"
                 << "  Functional axis mapped S->B by static offset: "
                 << distalAxisMappedByStatic.transpose() << '\n'
                 << "  Functional target axis B: " << axisAnalysis.targetAxes.distal.transpose()
                 << '\n'
                 << "  Static functional-axis disagreement: "
                 << unsignedAxisSeparationDegrees(distalAxisMappedByStatic,
                                                  axisAnalysis.targetAxes.distal)
                 << " degrees\n"
                 << "  Gravity mapped S->B by static offset: "
                 << distalGravityMappedByStatic.transpose() << '\n'
                 << "  Gravity target B: " << distalTargetGravity.transpose() << '\n'
                 << "  Static gravity disagreement: "
                 << unsignedAxisSeparationDegrees(distalGravityMappedByStatic, distalTargetGravity)
                 << " degrees" << std::endl;

    // Compare only distinct functional movements; repeated sources are replaced.
    for (const CalibrationVectorObservation& existing : observations[distalSensor]) {

        if (existing.source == "Static-pose gravity" || existing.source == distalFunctionalSource) {
            continue;
        }

        const double targetSeparationDegrees =
            unsignedAxisSeparationDegrees(existing.targetVectorB, axisAnalysis.targetAxes.distal);

        const double measuredSeparationDegrees =
            unsignedAxisSeparationDegrees(existing.sensorVectorS, axisAnalysis.distalAxis.axis);

        errorOutput_ << "Observation-geometry check for " << joint.displayName << " against "
                     << existing.source << ":\n"
                     << "  Target-axis separation: " << targetSeparationDegrees << " degrees\n"
                     << "  Measured-axis separation: " << measuredSeparationDegrees << " degrees"
                     << std::endl;

        // Distinct anatomical targets should not produce nearly parallel measurements.
        if (targetSeparationDegrees >= 45.0 && measuredSeparationDegrees < 20.0) {

            frontend_.emitError(frontend_protocol::error::kFunctionalCalibration,
                                joint.displayName +
                                    ":New axis is inconsistent with previous observations");

            errorOutput_ << "Functional calibration rejected: the new movement "
                         << "should provide a distinct anatomical axis, but its "
                         << "measured axis is nearly parallel to " << existing.source << "."
                         << std::endl;

            return std::nullopt;
        }

        // A redundant target is valid but contributes little orientation information.
        if (targetSeparationDegrees < 20.0) {
            errorOutput_ << "Warning: the new observation is anatomically redundant "
                         << "with " << existing.source << "." << std::endl;
        }
    }

    replaceObservationBySource(
        observations[distalSensor],
        CalibrationVectorObservation{
            axisAnalysis.distalAxis.axis, axisAnalysis.targetAxes.distal, distalFunctionalWeight,
            true, distalFunctionalSource,
            effectiveFunctionalUncertainty(axisAnalysis.distalAxis.angularUncertaintyRadians)});
    constexpr double RADIANS_TO_DEGREES = 180.0 / 3.14159265358979323846;

    errorOutput_
        << "Uncertainty-based distal Wahba weights:\n"
        << "  Gravity weight: " << GRAVITY_WEIGHT << '\n'
        << "  Measured gravity sigma: "
        << capture.distalGravity.angularUncertaintyRadians * RADIANS_TO_DEGREES << " degrees\n"
        << "  Session gravity floor: "
        << calibrationSession.gravityUncertaintyFloorRadians() * RADIANS_TO_DEGREES << " degrees\n"
        << "  Gravity absolute floor: " << MINIMUM_GRAVITY_UNCERTAINTY_RADIANS * RADIANS_TO_DEGREES
        << " degrees\n"
        << "  Gravity effective sigma: "
        << effectiveGravityUncertainty(capture.distalGravity.angularUncertaintyRadians,
                                       calibrationSession.gravityUncertaintyFloorRadians()) *
               RADIANS_TO_DEGREES
        << " degrees\n"
        << "  Measured functional sigma: "
        << axisAnalysis.distalAxis.angularUncertaintyRadians * RADIANS_TO_DEGREES << " degrees\n"
        << "  Functional effective sigma: "
        << effectiveFunctionalUncertainty(axisAnalysis.distalAxis.angularUncertaintyRadians) *
               RADIANS_TO_DEGREES
        << " degrees\n"
        << "  Raw functional weight: " << distalRawFunctionalWeight << '\n'
        << "  Applied functional weight: " << distalFunctionalWeight;

    if (axisAnalysis.refinesProximalSegment) {
        const double proximalFunctionalWeight =
            functionalWeightRelativeToGravity(capture.proximalGravity.angularUncertaintyRadians,
                                              calibrationSession.gravityUncertaintyFloorRadians(),
                                              axisAnalysis.proximalAxis.angularUncertaintyRadians);
        const double proximalRawFunctionalWeight = rawFunctionalWeightRelativeToGravity(
            capture.proximalGravity.angularUncertaintyRadians,
            calibrationSession.gravityUncertaintyFloorRadians(),
            axisAnalysis.proximalAxis.angularUncertaintyRadians);

        if (!(proximalFunctionalWeight > 0.0) || !std::isfinite(proximalFunctionalWeight)) {

            frontend_.emitError(frontend_protocol::error::kFunctionalCalibration,
                                joint.displayName +
                                    ":Could not calculate a valid proximal observation weight");

            errorOutput_ << "Functional calibration rejected: invalid proximal "
                         << "uncertainty-based weight." << std::endl;

            return std::nullopt;
        }

        replaceObservationBySource(
            observations[proximalSensor],
            CalibrationVectorObservation{
                capture.proximalGravity.direction, proximalTargetGravity, GRAVITY_WEIGHT, true,
                "Static-pose gravity",
                effectiveGravityUncertainty(capture.proximalGravity.angularUncertaintyRadians,
                                            calibrationSession.gravityUncertaintyFloorRadians())});
        replaceObservationBySource(
            observations[proximalSensor],
            CalibrationVectorObservation{axisAnalysis.proximalAxis.axis,
                                         axisAnalysis.targetAxes.proximal, proximalFunctionalWeight,
                                         true, joint.displayName + " flexion/extension axis",
                                         effectiveFunctionalUncertainty(
                                             axisAnalysis.proximalAxis.angularUncertaintyRadians)});
        errorOutput_
            << "Uncertainty-based proximal Wahba weights:\n"
            << "  Gravity weight: " << GRAVITY_WEIGHT << '\n'
            << "  Measured gravity sigma: "
            << capture.proximalGravity.angularUncertaintyRadians * RADIANS_TO_DEGREES
            << " degrees\n"
            << "  Session gravity floor: "
            << calibrationSession.gravityUncertaintyFloorRadians() * RADIANS_TO_DEGREES
            << " degrees\n"
            << "  Gravity absolute floor: "
            << MINIMUM_GRAVITY_UNCERTAINTY_RADIANS * RADIANS_TO_DEGREES << " degrees\n"
            << "  Gravity effective sigma: "
            << effectiveGravityUncertainty(capture.proximalGravity.angularUncertaintyRadians,
                                           calibrationSession.gravityUncertaintyFloorRadians()) *
                   RADIANS_TO_DEGREES
            << " degrees\n"
            << "  Measured functional sigma: "
            << axisAnalysis.proximalAxis.angularUncertaintyRadians * RADIANS_TO_DEGREES
            << " degrees\n"
            << "  Functional effective sigma: "
            << effectiveFunctionalUncertainty(axisAnalysis.proximalAxis.angularUncertaintyRadians) *
                   RADIANS_TO_DEGREES
            << " degrees\n"
            << "  Raw functional weight: " << proximalRawFunctionalWeight << '\n'
            << "  Applied functional weight: " << proximalFunctionalWeight << '\n';

        const Eigen::Quaterniond& proximalStaticOffset =
            calibrationSession.staticOffsets().at(proximalSensor);

        const Eigen::Vector3d proximalAxisMappedByStatic =
            proximalStaticOffset * axisAnalysis.proximalAxis.axis;

        const Eigen::Vector3d proximalGravityMappedByStatic =
            proximalStaticOffset * capture.proximalGravity.direction;

        errorOutput_ << "Proximal frame-consistency diagnostics:\n"
                     << "  Functional axis mapped S->B by static offset: "
                     << proximalAxisMappedByStatic.transpose() << '\n'
                     << "  Functional target axis B: "
                     << axisAnalysis.targetAxes.proximal.transpose() << '\n'
                     << "  Static functional-axis disagreement: "
                     << unsignedAxisSeparationDegrees(proximalAxisMappedByStatic,
                                                      axisAnalysis.targetAxes.proximal)
                     << " degrees\n"
                     << "  Gravity mapped S->B by static offset: "
                     << proximalGravityMappedByStatic.transpose() << '\n'
                     << "  Gravity target B: " << proximalTargetGravity.transpose() << '\n'
                     << "  Static gravity disagreement: "
                     << unsignedAxisSeparationDegrees(proximalGravityMappedByStatic,
                                                      proximalTargetGravity)
                     << " degrees" << std::endl;
    }

    const double distalSeparation = unsignedAxisSeparationDegrees(capture.distalGravity.direction,
                                                                  axisAnalysis.distalAxis.axis);

    errorOutput_ << "Current-capture distal gravity-axis separation: " << distalSeparation
                 << " degrees" << std::endl;

    if (distalSeparation < MINIMUM_AXIS_SEPARATION_DEGREES) {
        frontend_.emitError(frontend_protocol::error::kFunctionalCalibration,
                            joint.displayName + ":Gravity and functional axis are too parallel");
        errorOutput_ << "Functional calibration rejected. "
                     << "Distal gravity-axis separation: " << distalSeparation << " degrees."
                     << std::endl;
        return std::nullopt;
    }

    if (axisAnalysis.refinesProximalSegment &&
        unsignedAxisSeparationDegrees(capture.proximalGravity.direction,
                                      axisAnalysis.proximalAxis.axis) <
            MINIMUM_AXIS_SEPARATION_DEGREES) {

        frontend_.emitError(frontend_protocol::error::kFunctionalCalibration,
                            joint.displayName +
                                ":Proximal gravity and functional axis are too parallel");
        return std::nullopt;
    }

    FunctionalRefinementCandidate candidate;
    candidate.observations = std::move(observations);
    errorOutput_ << "\nComplete accumulated geometry for " << joint.displayName << ":\n";

    const auto& distalObservations = candidate.observations.at(distalSensor);

    for (std::size_t first = 0; first < distalObservations.size(); ++first) {

        for (std::size_t second = first + 1; second < distalObservations.size(); ++second) {

            const auto& a = distalObservations[first];
            const auto& b = distalObservations[second];

            errorOutput_ << "  " << a.source << " versus " << b.source << ":\n"
                         << "    Measured separation: "
                         << unsignedAxisSeparationDegrees(a.sensorVectorS, b.sensorVectorS)
                         << " degrees\n"
                         << "    Target separation: "
                         << unsignedAxisSeparationDegrees(a.targetVectorB, b.targetVectorB)
                         << " degrees\n";
        }
    }
    candidate.distalResult = SeatedCalibration::refineStaticOffsetFromObservations(
        calibrationSession.staticOffsets().at(distalSensor),
        candidate.observations.at(distalSensor));

    if (!axisAnalysis.refinesProximalSegment) {
        candidate.proximalResult.valid = true;
        candidate.proximalResult.offset = calibrationSession.finalOffsets().at(proximalSensor);

        candidate.proximalResult.message =
            axisAnalysis.isPronationSupination
                ? "Upper-arm offset preserved during forearm rotation."
                : "Sternum offset preserved during shoulder movement.";
    } else {
        candidate.proximalResult = SeatedCalibration::refineStaticOffsetFromObservations(
            calibrationSession.staticOffsets().at(proximalSensor),
            candidate.observations.at(proximalSensor));
    }

    if (!candidate.proximalResult.valid || !candidate.distalResult.valid ||
        candidate.distalResult.maximumResidualDegrees > MAXIMUM_RESIDUAL_DEGREES ||
        (axisAnalysis.refinesProximalSegment &&
         candidate.proximalResult.maximumResidualDegrees > MAXIMUM_RESIDUAL_DEGREES)) {

        frontend_.emitError(frontend_protocol::error::kFunctionalCalibration,
                            joint.displayName + ":Combined observation solve rejected");
        errorOutput_ << "Combined functional calibration rejected for " << joint.displayName << "\n"
                     << "Proximal: " << candidate.proximalResult.message << "\n"
                     << "Proximal maximum residual: "
                     << candidate.proximalResult.maximumResidualDegrees << " degrees\n"
                     << "Distal: " << candidate.distalResult.message << "\n"
                     << "Distal maximum residual: " << candidate.distalResult.maximumResidualDegrees
                     << " degrees" << std::endl;

        return std::nullopt;
    }

    return candidate;
}
