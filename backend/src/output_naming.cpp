#include "output_naming.h"

#include <cctype>

namespace output_naming {

std::string safeFilenamePart(const std::string& text) {
    std::string result;
    bool previousWasUnderscore = false;
    for (const unsigned char character : text) {
        if (std::isalnum(character) || character == '-' || character == '_') {
            result.push_back(static_cast<char>(character));
            previousWasUnderscore = false;
        } else if (!previousWasUnderscore) {
            result.push_back('_');
            previousWasUnderscore = true;
        }
    }

    const auto firstUsableCharacter = result.find_first_not_of('_');
    if (firstUsableCharacter == std::string::npos) {
        return "Session";
    }
    result.erase(0, firstUsableCharacter);

    while (!result.empty() && result.back() == '_') {
        result.pop_back();
    }
    return result;
}

std::filesystem::path nextMeasurementFilename(const std::filesystem::path& outputDirectory,
                                              const std::string& sessionName) {

    for (std::size_t trial = 1;; ++trial) {
        const std::filesystem::path candidate =
            outputDirectory / (sessionName + "_Measurement_" + std::to_string(trial) + ".csv");

        if (!std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
}

std::filesystem::path staticCalibrationFilename(const std::filesystem::path& outputDirectory,
                                                const std::string& sessionName,
                                                const std::string& poseFileLabel,
                                                const std::size_t captureNumber) {

    return outputDirectory /
           (sessionName + "_Static" + poseFileLabel + "_" + std::to_string(captureNumber) + ".csv");
}

std::filesystem::path functionalCalibrationFilename(const std::filesystem::path& outputDirectory,
                                                    const std::string& sessionName,
                                                    const std::string& jointFileLabel,
                                                    const std::size_t captureNumber) {

    return outputDirectory / (sessionName + "_Functional_" + jointFileLabel + "_" +
                              std::to_string(captureNumber) + ".csv");
}

} // namespace output_naming
