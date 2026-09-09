#pragma once

#include <cstddef>
#include <optional>
#include <vector>

namespace calibration_statistics {

/**
 * Returns the median of the finite values.
 * @param values Values to filter and summarize.
 * @return Median of the finite values, or no value when none are finite.
 */
std::optional<double> median(const std::vector<double>& values);

/**
 * Converts median absolute deviation into a robust standard-deviation-like
 * angular uncertainty:
 *
 * sigma = 1.4826 * median(|x - median(x)|)
 *
 * Input and output use the same units. For calibration, pass radians.
 * @param angularValues Angular samples to summarize.
 * @param minimumValueCount Minimum number of finite samples required.
 * @return Robust uncertainty estimate, or no value when too few samples are finite.
 */
std::optional<double> robustMadSigma(const std::vector<double>& angularValues,
                                     std::size_t minimumValueCount = 4);

} // namespace calibration_statistics
