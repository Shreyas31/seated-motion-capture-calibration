#include "realtime_viewer_configuration.h"

#include <charconv>
#include <stdexcept>
#include <string_view>
#include <system_error>

namespace SeatedMoCap {
namespace {

constexpr int kMinimumArgumentCount = 5;
constexpr int kMaximumArgumentCount = 8;

int parsePositiveDuration(const std::string_view text) {
    int duration = 0;
    const auto* begin = text.data();
    const auto* end = begin + text.size();
    const auto result = std::from_chars(begin, end, duration);

    if (result.ec != std::errc{} || result.ptr != end || duration <= 0) {
        throw std::runtime_error("Listening duration must be a positive whole number.");
    }

    return duration;
}

RootTranslationMode parseRootTranslationMode(const std::string_view text) {
    if (text == "free-root") {
        return RootTranslationMode::FreeRoot;
    }

    if (text == "stationary-feet") {
        return RootTranslationMode::StationaryFeet;
    }

    throw std::runtime_error("Translation mode must be either 'stationary-feet' or 'free-root'.");
}

} // namespace

bool RealtimeViewerConfiguration::stationaryFeetEnabled() const noexcept {
    return rootTranslationMode == RootTranslationMode::StationaryFeet;
}

bool isRealtimeViewerArgumentCountValid(const int argumentCount) noexcept {
    return argumentCount >= kMinimumArgumentCount && argumentCount <= kMaximumArgumentCount;
}

std::string realtimeViewerUsage() {
    return "Usage:\n"
           "  opensim_rt_viewer <model.osim> <pose_presets.json> "
           "<pose-preset-name> <geometry-directory> "
           "[listening-duration-seconds] [stationary-feet|free-root] "
           "[joint-angle-output.csv]\n";
}

RealtimeViewerConfiguration parseRealtimeViewerConfiguration(const int argumentCount,
                                                             const char* const arguments[]) {
    if (!isRealtimeViewerArgumentCountValid(argumentCount)) {
        throw std::runtime_error("Unexpected number of real-time viewer arguments.");
    }

    RealtimeViewerConfiguration configuration;
    configuration.modelPath = arguments[1];
    configuration.presetPath = arguments[2];
    configuration.poseName = arguments[3];
    configuration.geometryPath = arguments[4];

    if (argumentCount >= 6) {
        configuration.listeningDurationSeconds = parsePositiveDuration(arguments[5]);
    }

    if (argumentCount >= 7) {
        configuration.rootTranslationMode = parseRootTranslationMode(arguments[6]);
    }

    if (argumentCount == 8) {
        configuration.jointAngleOutputPath = arguments[7];
    }

    return configuration;
}

void validateRealtimeViewerInputPaths(const RealtimeViewerConfiguration& configuration) {
    if (!std::filesystem::is_regular_file(configuration.modelPath)) {
        throw std::runtime_error("Model file does not exist: " + configuration.modelPath.string());
    }

    if (!std::filesystem::is_directory(configuration.geometryPath)) {
        throw std::runtime_error("Geometry directory does not exist: " +
                                 configuration.geometryPath.string());
    }
}

} // namespace SeatedMoCap
