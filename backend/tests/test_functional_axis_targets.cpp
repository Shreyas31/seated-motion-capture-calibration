#include "functional_calibration_solver.h"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void expect(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testKneeUsesConfiguredAxes() {
    const calibration::JointCalibrationDefinition knee{
        "Right knee",
        "Right_Upperleg",
        "Right_Lowerleg",
        calibration::FunctionalMovementType::FlexionExtension,
        Eigen::Vector3d::UnitX(),
        Eigen::Vector3d::UnitZ()};
    const auto targets = buildFunctionalTargetAxes(knee);
    expect(targets.proximal.isApprox(Eigen::Vector3d::UnitX()),
           "Configured proximal knee axis changed.");
    expect(targets.distal.isApprox(Eigen::Vector3d::UnitZ()),
           "Configured distal knee axis changed.");
}

void testPronationUsesForearmLongitudinalAxis() {
    const calibration::JointCalibrationDefinition pronation{
        "Right forearm pronation/supination", "Right_Upperarm", "Right_Forearm",
        calibration::FunctionalMovementType::PronationSupination};
    const auto targets = buildFunctionalTargetAxes(pronation);
    expect(targets.distal.isApprox(Eigen::Vector3d::UnitY()),
           "Pronation target was not forearm +Y.");
}

void testFlexionUsesForearmXAxis() {
    const calibration::JointCalibrationDefinition elbow{
        "Left elbow", "Left_Upperarm", "Left_Forearm",
        calibration::FunctionalMovementType::FlexionExtension};
    const auto targets = buildFunctionalTargetAxes(elbow);
    expect(targets.distal.isApprox(Eigen::Vector3d::UnitX()), "Flexion target was not forearm +X.");
}

} // namespace

int main() {
    try {
        testKneeUsesConfiguredAxes();
        testPronationUsesForearmLongitudinalAxis();
        testFlexionUsesForearmXAxis();
    } catch (const std::exception& error) {
        std::cerr << "Functional axis target test failed: " << error.what() << std::endl;
        return 1;
    }
    std::cout << "Functional axis target tests passed." << std::endl;
    return 0;
}
