#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

namespace output_naming {

/**
 * Converts arbitrary user text into a portable, readable filename component.
 * @param text Session or calibration label to sanitise.
 * @return Sanitised component, or "Session" when no usable characters remain.
 */
std::string safeFilenamePart(const std::string& text);

/**
 * Finds the first unused numbered measurement CSV path.
 * @param outputDirectory Directory in which candidate files are checked.
 * @param sessionName Sanitised session name used as the filename prefix.
 * @return First available path following the session Measurement numbering convention.
 */
std::filesystem::path nextMeasurementFilename(const std::filesystem::path& outputDirectory,
                                              const std::string& sessionName);

/**
 * Constructs the path for one static-calibration capture.
 * @param outputDirectory Directory in which the CSV will be written.
 * @param sessionName Sanitised session name used as the filename prefix.
 * @param poseFileLabel Pose and palm-orientation label included in the filename.
 * @param captureNumber One-based capture number.
 * @return Path following the static-calibration filename convention.
 */
std::filesystem::path staticCalibrationFilename(const std::filesystem::path& outputDirectory,
                                                const std::string& sessionName,
                                                const std::string& poseFileLabel,
                                                std::size_t captureNumber);

/**
 * Constructs the path for one functional-calibration capture.
 * @param outputDirectory Directory in which the CSV will be written.
 * @param sessionName Sanitised session name used as the filename prefix.
 * @param jointFileLabel Joint or movement label included in the filename.
 * @param captureNumber One-based capture number.
 * @return Path following the functional-calibration filename convention.
 */
std::filesystem::path functionalCalibrationFilename(const std::filesystem::path& outputDirectory,
                                                    const std::string& sessionName,
                                                    const std::string& jointFileLabel,
                                                    std::size_t captureNumber);

} // namespace output_naming
