#include "calibration_sample_analysis.h"

#include <Eigen>
#include <cmath>
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

void testQuaternionDistanceHandlesEquivalentSigns() {
    const Eigen::Quaterniond rotation{Eigen::AngleAxisd{0.8, Eigen::Vector3d::UnitY()}};
    const Eigen::Quaterniond negative{-rotation.w(), -rotation.x(), -rotation.y(), -rotation.z()};

    expect(calibration_analysis::quaternionAngularDistanceDegrees(rotation, negative) < 1e-9,
           "Quaternion sign equivalence was not handled.");
}

} // namespace

int main() {
    try {
        testQuaternionDistanceHandlesEquivalentSigns();
    } catch (const std::exception& error) {
        std::cerr << "Calibration sample analysis test failed: " << error.what() << std::endl;
        return 1;
    }

    std::cout << "Calibration sample analysis tests passed." << std::endl;
    return 0;
}
