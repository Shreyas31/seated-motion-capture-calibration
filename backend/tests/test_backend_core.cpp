#include "calibration_session.h"
#include "output_naming.h"
#include "seated_calibration.h"
#include "seated_pose_targets.h"
#include "sensor_mapping.h"

#include <Eigen>
#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void expect(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

double quaternionDistanceDegrees(const Eigen::Quaterniond& first,
                                 const Eigen::Quaterniond& second) {

    const Eigen::Quaterniond difference =
        (first.normalized().conjugate() * second.normalized()).normalized();

    const double scalar = std::clamp(std::abs(difference.w()), 0.0, 1.0);

    return 2.0 * std::acos(scalar) * 180.0 / std::acos(-1.0);
}

void testStaticCalibrationRoundTrip() {
    const Eigen::Quaterniond q_CS{Eigen::AngleAxisd{0.7, Eigen::Vector3d::UnitZ()}};
    const Eigen::Quaterniond q_CB{Eigen::AngleAxisd{-0.4, Eigen::Vector3d::UnitY()}};

    const Eigen::Quaterniond q_BS = SeatedCalibration::computeStaticOffset(q_CS, q_CB);

    const Eigen::Quaterniond recovered = SeatedCalibration::applyCalibration(q_CS, q_BS);

    expect(quaternionDistanceDegrees(recovered, q_CB) < 1e-9,
           "Static calibration did not recover q_CB.");
}

void testQuaternionMeanHandlesEquivalentSigns() {
    const Eigen::Quaterniond expected{Eigen::AngleAxisd{0.8, Eigen::Vector3d::UnitX()}};
    const Eigen::Quaterniond equivalentNegative{-expected.w(), -expected.x(), -expected.y(),
                                                -expected.z()};

    const auto mean =
        SeatedCalibration::averageQuaternions({expected, equivalentNegative, expected});

    expect(mean.has_value(), "Quaternion mean was not produced.");
    expect(quaternionDistanceDegrees(*mean, expected) < 1e-9,
           "Quaternion mean mishandled q/-q equivalence.");
}

void testFunctionalAxisUncertainty() {
    std::vector<Eigen::Vector3d> samples;

    // Simulate five chronological movement blocks with small differences in
    // their measured hinge directions.
    const std::array<double, 5> block_offsets = {-0.04, -0.02, 0.0, 0.02, 0.08};

    for (std::size_t block = 0; block < block_offsets.size(); ++block) {

        const Eigen::Vector3d axis =
            Eigen::AngleAxisd(block_offsets[block], Eigen::Vector3d::UnitZ()) *
            Eigen::Vector3d::UnitY();

        for (std::size_t sample = 0; sample < 40; ++sample) {
            // Alternating sign represents flexion and extension. PCA should
            // identify the same unoriented hinge line from both directions.
            const double direction = (sample % 2 == 0) ? 1.0 : -1.0;

            const double small_noise = 0.002 * std::sin(static_cast<double>(sample));

            samples.push_back(direction * 1.2 * axis + small_noise * Eigen::Vector3d::UnitZ());
        }
    }

    const FunctionalAxisEstimate estimate =
        SeatedCalibration::computeFunctionalAxisWithUncertainty(samples);

    expect(estimate.valid, "Functional-axis uncertainty estimate should be valid.");

    expect(estimate.uncertaintyBlocks == 5, "All five functional-axis blocks should be valid.");

    expect(std::isfinite(estimate.angularUncertaintyRadians),
           "Functional-axis uncertainty should be finite.");

    expect(estimate.angularUncertaintyRadians > 0.0 && estimate.angularUncertaintyRadians < 0.2,
           "Functional-axis uncertainty should be small but non-zero.");
}

void testFunctionalAxisEstimate() {
    std::vector<Eigen::Vector3d> samples;
    samples.reserve(40);

    for (int index = 0; index < 40; ++index) {
        const double direction = index % 2 == 0 ? 1.0 : -1.0;
        samples.emplace_back(0.01 * std::sin(static_cast<double>(index)), direction * 1.2,
                             0.01 * std::cos(static_cast<double>(index)));
    }

    const FunctionalAxisEstimate result = SeatedCalibration::computeFunctionalAxis(samples);

    expect(result.valid, "Clean hinge movement was rejected.");
    expect(std::abs(result.axis.dot(Eigen::Vector3d::UnitY())) > 0.999,
           "Functional axis did not align with the expected hinge line.");
    expect(result.confidence > 0.99, "Functional-axis confidence was low.");
}

void testGravityEstimate() {
    /*
     * Four half-second blocks of 30 samples at 60 Hz. The direction is constant, so the
     * uncertainty should be approximately zero.
     */
    std::vector<Eigen::Vector3d> samples(120, Eigen::Vector3d{0.0, 0.0, -9.81});

    const GravityDirectionEstimate gravity = SeatedCalibration::computeGravityDirection(samples);

    expect(gravity.valid, "Valid gravity samples were rejected.");

    expect(gravity.direction.dot(-Eigen::Vector3d::UnitZ()) > 0.999999,
           "Gravity estimate has the wrong direction.");

    expect(gravity.usedBlocks == 4, "Gravity estimate used the wrong number of blocks.");

    expect(gravity.angularUncertaintyRadians < 1e-12,
           "Constant gravity unexpectedly produced uncertainty.");
}

std::vector<CalibrationVectorObservation>
observationsForRotation(const Eigen::Quaterniond& sensorToBody) {

    std::vector<CalibrationVectorObservation> observations;
    for (const Eigen::Vector3d& sensorAxis :
         {Eigen::Vector3d::UnitX(), Eigen::Vector3d::UnitY(), Eigen::Vector3d::UnitZ()}) {

        CalibrationVectorObservation observation;
        observation.sensorVectorS = sensorAxis;
        observation.targetVectorB = sensorToBody * sensorAxis;
        observation.weight = 1.0;
        observations.push_back(observation);
    }
    return observations;
}

void testIndependentWahbaAppliesCompleteAcceptedCorrection() {
    const Eigen::Quaterniond expected{
        Eigen::AngleAxisd{30.0 * std::acos(-1.0) / 180.0, Eigen::Vector3d::UnitZ()}};

    const FunctionalRefinementResult result = SeatedCalibration::refineStaticOffsetFromObservations(
        Eigen::Quaterniond::Identity(), observationsForRotation(expected));

    expect(result.valid, "A 30-degree independent Wahba correction was rejected.");
    expect(quaternionDistanceDegrees(result.offset, expected) < 1e-9,
           "The accepted Wahba correction was not applied completely.");
    expect(std::abs(result.correctionDegrees - 30.0) < 1e-9,
           "The accepted Wahba correction diagnostic is incorrect.");
}

void testIndependentWahbaRejectsCorrectionBeyondSixtyDegrees() {
    const Eigen::Quaterniond excessive{
        Eigen::AngleAxisd{61.0 * std::acos(-1.0) / 180.0, Eigen::Vector3d::UnitZ()}};

    const FunctionalRefinementResult result = SeatedCalibration::refineStaticOffsetFromObservations(
        Eigen::Quaterniond::Identity(), observationsForRotation(excessive));

    expect(!result.valid, "A Wahba correction above 60 degrees was accepted.");
    expect(quaternionDistanceDegrees(result.offset, Eigen::Quaterniond::Identity()) < 1e-9,
           "A rejected Wahba correction changed the static offset.");
    expect(result.correctionDegrees > 60.0,
           "The rejected Wahba correction diagnostic was not retained.");
}

void expectPoseTargets(const std::map<std::string, Eigen::Quaterniond>& targets,
                       const std::string& poseName) {

    expect(targets.size() == SensorMapping::canonicalSegments().size(),
           poseName + " does not define every canonical segment.");
    for (const auto& [segment, orientation] : targets) {
        expect(orientation.coeffs().allFinite(),
               poseName + " has a non-finite target for " + segment + ".");
        expect(std::abs(orientation.norm() - 1.0) < 1e-12,
               poseName + " has a non-unit target for " + segment + ".");
    }
}

void testPoseTargets() {
    expectPoseTargets(buildChairPoseTargets(), "Chair pose");
    expectPoseTargets(buildBedPoseTargets(), "Bed pose");
}

void testCalibrationSessionLifecycle() {
    CalibrationSession session;

    std::map<std::string, Eigen::Quaterniond> offsets{{"sensor-a", Eigen::Quaterniond::Identity()},
                                                      {"sensor-b", Eigen::Quaterniond::Identity()}};
    std::map<std::string, Eigen::Vector3d> biases{{"sensor-a", Eigen::Vector3d::Zero()},
                                                  {"sensor-b", Eigen::Vector3d::Zero()}};

    GravityDirectionEstimate gravityA;
    gravityA.valid = true;
    gravityA.direction = -Eigen::Vector3d::UnitZ();

    gravityA.angularUncertaintyRadians = 0.01;

    gravityA.usedBlocks = 4;
    gravityA.message = "Test gravity estimate.";

    GravityDirectionEstimate gravityB = gravityA;

    std::map<std::string, GravityDirectionEstimate> gravityEstimates{{"sensor-a", gravityA},
                                                                     {"sensor-b", gravityB}};

    constexpr double gravityFloor = 0.01;

    session.commitStaticCalibration(offsets, biases, gravityEstimates, gravityFloor,
                                    Eigen::Quaterniond::Identity());

    expect(session.hasStaticCalibration(), "Static commit was not retained.");
    expect(session.state() == CalibrationState::StaticReady,
           "Static commit produced the wrong state.");
    expect(session.finalOffsets().size() == offsets.size(),
           "Static commit did not initialise final offsets.");

    expect(session.staticGravityEstimates().size() == offsets.size(),
           "Static commit did not retain gravity estimates.");
    expect(std::abs(session.gravityUncertaintyFloorRadians() - gravityFloor) < 1e-12,
           "Static commit retained the wrong gravity floor.");

    session.invalidateStaticCalibration();

    expect(session.staticGravityEstimates().empty(), "Invalidation retained gravity estimates.");
    expect(session.gravityUncertaintyFloorRadians() == 0.0,
           "Invalidation retained the gravity floor.");

    expect(!session.hasStaticCalibration(), "Invalidation retained calibration.");
    expect(session.staticOffsets().empty(), "Invalidation retained offsets.");
    expect(session.gyroBiases().empty(), "Invalidation retained gyro biases.");
}

void testPortableOutputName() {
    expect(output_naming::safeFilenamePart(" P01 / chair ") == "P01_chair",
           "Unexpected portable filename conversion.");
    expect(output_naming::safeFilenamePart("***") == "Session",
           "Empty portable filename did not use the fallback.");
}

void testStaticCalibrationOutputName() {
    const std::filesystem::path filename =
        output_naming::staticCalibrationFilename("exports", "P01", "Chair_PalmsTogether", 2);

    expect(filename == std::filesystem::path{"exports/P01_StaticChair_PalmsTogether_2.csv"},
           "Static calibration filename changed.");
}

void testFunctionalCalibrationOutputName() {
    const std::filesystem::path filename =
        output_naming::functionalCalibrationFilename("exports", "P01", "Right_elbow", 3);

    expect(filename == std::filesystem::path{"exports/P01_Functional_Right_elbow_3.csv"},
           "Functional calibration filename changed.");
}

} // namespace

int main() {
    try {
        testStaticCalibrationRoundTrip();
        testQuaternionMeanHandlesEquivalentSigns();
        testFunctionalAxisEstimate();
        testFunctionalAxisUncertainty();
        testGravityEstimate();
        testIndependentWahbaAppliesCompleteAcceptedCorrection();
        testIndependentWahbaRejectsCorrectionBeyondSixtyDegrees();
        testPoseTargets();
        testCalibrationSessionLifecycle();
        testPortableOutputName();
        testStaticCalibrationOutputName();
        testFunctionalCalibrationOutputName();

        std::cout << "Backend core tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Backend core test failed: " << error.what() << '\n';
        return 1;
    }
}
