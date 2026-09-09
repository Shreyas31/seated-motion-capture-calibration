#include "angular_uncertainty.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

void require(bool condition, const char* message) {

    if (!condition) {
        throw std::runtime_error(message);
    }
}

} // namespace

int main() {
    try {
        const double degreesToRadians = std::acos(-1.0) / 180.0;

        const std::vector<double> angles{1.0 * degreesToRadians, 2.0 * degreesToRadians,
                                         2.0 * degreesToRadians, 3.0 * degreesToRadians,
                                         15.0 * degreesToRadians};

        const auto result = calibration_statistics::robustMadSigma(angles);

        require(result.has_value(), "MAD sigma was not calculated.");

        /*
         * Median = 2 degrees.
         * Absolute deviations = [1, 0, 0, 1, 13] degrees.
         * MAD = 1 degree.
         * Robust sigma = 1.4826 degrees.
         */
        const double expected = 1.4826 * degreesToRadians;

        require(std::abs(*result - expected) < 1e-10, "MAD sigma has the wrong value.");

        const auto tooFew = calibration_statistics::robustMadSigma(
            {1.0 * degreesToRadians, 2.0 * degreesToRadians, 3.0 * degreesToRadians});

        require(!tooFew.has_value(), "Fewer than four values should be rejected.");

        std::cout << "Angular uncertainty tests passed.\n";

        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "Angular uncertainty test failed: " << exception.what() << '\n';

        return 1;
    }
}
