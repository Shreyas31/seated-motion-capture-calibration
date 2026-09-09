#include "functional_calibration_solver.h"

#include <cmath>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void expect(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testObservationReplacementUsesStableSource() {
    std::vector<CalibrationVectorObservation> observations{
        {Eigen::Vector3d::UnitX(), Eigen::Vector3d::UnitX(), 1.0, true, "gravity"},
        {Eigen::Vector3d::UnitY(), Eigen::Vector3d::UnitY(), 0.8, true, "axis"}};
    replaceObservationBySource(
        observations, {Eigen::Vector3d::UnitZ(), Eigen::Vector3d::UnitZ(), 0.9, true, "axis"});

    expect(observations.size() == 2, "Repeated observation accumulated extra weight.");
    expect(observations.back().source == "axis" &&
               observations.back().sensorVectorS.isApprox(Eigen::Vector3d::UnitZ()),
           "Replacement observation was not installed.");
}

void testUnsignedAxisSeparationIgnoresSign() {
    expect(std::abs(unsignedAxisSeparationDegrees(Eigen::Vector3d::UnitX(),
                                                  -Eigen::Vector3d::UnitX())) < 1e-9,
           "Opposite axis directions were not treated as one line.");
    expect(
        std::abs(unsignedAxisSeparationDegrees(Eigen::Vector3d::UnitX(), Eigen::Vector3d::UnitY()) -
                 90.0) < 1e-9,
        "Perpendicular axis separation was incorrect.");
}

} // namespace

int main() {
    try {
        testObservationReplacementUsesStableSource();
        testUnsignedAxisSeparationIgnoresSign();
    } catch (const std::exception& error) {
        std::cerr << "Functional observation test failed: " << error.what() << std::endl;
        return 1;
    }
    std::cout << "Functional observation tests passed." << std::endl;
    return 0;
}
