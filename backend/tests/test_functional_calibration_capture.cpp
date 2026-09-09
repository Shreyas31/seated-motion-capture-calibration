#include "functional_calibration_capture.h"

#include <exception>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

namespace {

void expect(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

const calibration::JointCalibrationDefinition JOINT{
    "Right elbow", "Right_Upperarm", "Right_Forearm",
    calibration::FunctionalMovementType::FlexionExtension};

void testCompleteCalibratedPairResolves() {
    const std::map<std::string, std::string> mapping{{"upper-id", "Right_Upperarm"},
                                                     {"forearm-id", "Right_Forearm"}};
    const std::map<std::string, Eigen::Quaterniond> offsets{
        {"upper-id", Eigen::Quaterniond::Identity()},
        {"forearm-id", Eigen::Quaterniond::Identity()}};
    const std::map<std::string, Eigen::Vector3d> biases{{"upper-id", Eigen::Vector3d::Zero()},
                                                        {"forearm-id", Eigen::Vector3d::Zero()}};

    const auto pair = resolveFunctionalSensorPair(JOINT, mapping, offsets, offsets, biases);
    expect(pair.has_value(), "Complete calibrated pair was rejected.");
    expect(pair->proximalSensor == "upper-id", "Proximal sensor resolved incorrectly.");
    expect(pair->distalSensor == "forearm-id", "Distal sensor resolved incorrectly.");
}

void testMissingMappingOrCalibrationIsRejected() {
    const std::map<std::string, std::string> incompleteMapping{{"upper-id", "Right_Upperarm"}};
    const std::map<std::string, Eigen::Quaterniond> upperOffset{
        {"upper-id", Eigen::Quaterniond::Identity()}};
    const std::map<std::string, Eigen::Vector3d> upperBias{{"upper-id", Eigen::Vector3d::Zero()}};
    expect(
        !resolveFunctionalSensorPair(JOINT, incompleteMapping, upperOffset, upperOffset, upperBias),
        "Missing distal mapping was accepted.");

    const std::map<std::string, std::string> completeMapping{{"upper-id", "Right_Upperarm"},
                                                             {"forearm-id", "Right_Forearm"}};
    expect(
        !resolveFunctionalSensorPair(JOINT, completeMapping, upperOffset, upperOffset, upperBias),
        "Uncalibrated distal sensor was accepted.");
}

} // namespace

int main() {
    try {
        testCompleteCalibratedPairResolves();
        testMissingMappingOrCalibrationIsRejected();
    } catch (const std::exception& error) {
        std::cerr << "Functional calibration capture test failed: " << error.what() << std::endl;
        return 1;
    }
    std::cout << "Functional calibration capture tests passed." << std::endl;
    return 0;
}
