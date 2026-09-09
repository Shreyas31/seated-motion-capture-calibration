#include "coordinate_conventions.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

bool approximatelyEqual(double first, double second, double tolerance = 1e-12) {
    return std::abs(first - second) <= tolerance;
}

void expectVector(const SeatedMoCap::Coordinates::Vector3& actual,
                  const SeatedMoCap::Coordinates::Vector3& expected, const std::string& message) {
    if (!approximatelyEqual(actual.x, expected.x) || !approximatelyEqual(actual.y, expected.y) ||
        !approximatelyEqual(actual.z, expected.z)) {
        throw std::runtime_error(message);
    }
}

void expect(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

} // namespace

int main() {
    using namespace SeatedMoCap::Coordinates;

    // Patient forward remains forward.
    expectVector(sessionVectorToOpenSim({1.0, 0.0, 0.0}), {1.0, 0.0, 0.0},
                 "Patient-forward axis mapping failed.");

    // Patient left becomes OpenSim -Z because OpenSim +Z is right.
    expectVector(sessionVectorToOpenSim({0.0, 1.0, 0.0}), {0.0, 0.0, -1.0},
                 "Patient-left axis mapping failed.");

    // Session vertical becomes OpenSim +Y.
    expectVector(sessionVectorToOpenSim({0.0, 0.0, 1.0}), {0.0, 1.0, 0.0},
                 "Vertical axis mapping failed.");

    // A segment aligned with C should have orientation q_OC in OpenSim.
    const QuaternionWxyz quaternionOB = sessionBodyToOpenSim({1.0, 0.0, 0.0, 0.0});

    expect(approximatelyEqual(quaternionOB.w, kSqrtHalf) &&
               approximatelyEqual(quaternionOB.x, -kSqrtHalf) &&
               approximatelyEqual(quaternionOB.y, 0.0) && approximatelyEqual(quaternionOB.z, 0.0),
           "Session-to-OpenSim quaternion mapping failed.");

    std::cout << "Coordinate convention tests passed.\n";
    return 0;
}
