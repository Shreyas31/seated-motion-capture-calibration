#pragma once

#include <filesystem>
#include <string>

namespace SeatedMoCap {

/** Selects how global pelvis translation is handled during real-time IK. */
enum class RootTranslationMode {
    FreeRoot,
    StationaryFeet,
};

/**
 * Stores validated command-line settings required by a real-time viewer session.
 * @class RealtimeViewerConfiguration.
 */
struct RealtimeViewerConfiguration {
    std::filesystem::path modelPath;
    std::filesystem::path presetPath;
    std::string poseName;
    std::filesystem::path geometryPath;
    int listeningDurationSeconds = 120;
    RootTranslationMode rootTranslationMode = RootTranslationMode::FreeRoot;
    std::filesystem::path jointAngleOutputPath;

    /**
     * Reports whether stationary-foot pelvis translation correction is selected.
     * @return True for StationaryFeet mode; otherwise false.
     */
    [[nodiscard]] bool stationaryFeetEnabled() const noexcept;
};

/**
 * Checks whether the command line contains the supported number of arguments.
 * @param argumentCount Value of `argc`, including the executable name.
 * @return True for the required arguments plus up to three optional arguments.
 */
[[nodiscard]] bool isRealtimeViewerArgumentCountValid(int argumentCount) noexcept;

/**
 * Creates the command-line usage text for the real-time viewer executable.
 * @return Human-readable usage instructions.
 */
[[nodiscard]] std::string realtimeViewerUsage();

/**
 * Parses real-time viewer command-line arguments into typed configuration values.
 * @param argumentCount Value of `argc`, including the executable name.
 * @param arguments Command-line argument array in the order shown by realtimeViewerUsage().
 * @return Parsed viewer configuration; filesystem paths are not checked here.
 * @throws std::runtime_error for an unsupported argument count, duration, or translation mode.
 */
[[nodiscard]] RealtimeViewerConfiguration
parseRealtimeViewerConfiguration(int argumentCount, const char* const arguments[]);

/**
 * Confirms that the configured model file and geometry directory exist.
 * @param configuration Parsed viewer configuration containing the input paths.
 * @throws std::runtime_error when the model is not a file or geometry path is not a directory.
 */
void validateRealtimeViewerInputPaths(const RealtimeViewerConfiguration& configuration);

} // namespace SeatedMoCap
