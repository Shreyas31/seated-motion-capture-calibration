#include "angular_uncertainty.h"

#include <algorithm>
#include <cmath>

namespace calibration_statistics {
namespace {

double medianOfSorted(const std::vector<double>& sortedValues) {

    const std::size_t count = sortedValues.size();

    const std::size_t middle = count / 2;

    if (count % 2 == 1) {
        return sortedValues[middle];
    }

    return 0.5 * (sortedValues[middle - 1] + sortedValues[middle]);
}

} // namespace

std::optional<double> median(const std::vector<double>& values) {

    std::vector<double> finiteValues;
    finiteValues.reserve(values.size());

    for (const double value : values) {
        if (std::isfinite(value)) {
            finiteValues.push_back(value);
        }
    }

    if (finiteValues.empty()) {
        return std::nullopt;
    }

    std::sort(finiteValues.begin(), finiteValues.end());

    return medianOfSorted(finiteValues);
}

std::optional<double> robustMadSigma(const std::vector<double>& angularValues,
                                     std::size_t minimumValueCount) {

    std::vector<double> finiteValues;
    finiteValues.reserve(angularValues.size());

    for (const double value : angularValues) {
        if (std::isfinite(value)) {
            finiteValues.push_back(value);
        }
    }

    if (finiteValues.size() < minimumValueCount) {
        return std::nullopt;
    }

    std::sort(finiteValues.begin(), finiteValues.end());

    const double centre = medianOfSorted(finiteValues);

    std::vector<double> absoluteDeviations;
    absoluteDeviations.reserve(finiteValues.size());

    for (const double value : finiteValues) {
        absoluteDeviations.push_back(std::abs(value - centre));
    }

    std::sort(absoluteDeviations.begin(), absoluteDeviations.end());

    constexpr double NORMAL_MAD_SCALE = 1.4826;

    const double sigma = NORMAL_MAD_SCALE * medianOfSorted(absoluteDeviations);

    if (!std::isfinite(sigma)) {
        return std::nullopt;
    }

    return sigma;
}

} // namespace calibration_statistics
