#pragma once

#include <filesystem>
#include <optional>
#include <string>

/**
 * Resolves and prepares filesystem locations used for session output.
 * @class RuntimeConfiguration.
 */
class RuntimeConfiguration {
  public:
    /**
     * Creates configuration from SEATED_MOCAP_OUTPUT_DIR or a fallback directory.
     * @param fallbackDirectory Directory used when the environment variable is unset or empty.
     * @return Resolved runtime configuration.
     */
    static RuntimeConfiguration fromEnvironment(
        const std::filesystem::path& fallbackDirectory = std::filesystem::current_path());

    /**
     * Creates configuration from an explicitly supplied optional directory value.
     * @param configuredDirectory Configured directory text, or no value to use the fallback.
     * @param fallbackDirectory Directory used when configuredDirectory is absent or empty.
     * @return Resolved runtime configuration.
     */
    static RuntimeConfiguration
    fromOutputDirectoryValue(const std::optional<std::string>& configuredDirectory,
                             const std::filesystem::path& fallbackDirectory);

    /**
     * Returns the directory in which session files are written.
     * @return Reference to the configured output path.
     */
    const std::filesystem::path& outputDirectory() const noexcept;

    /**
     * Creates the output directory and any missing parent directories.
     * @throws std::filesystem::filesystem_error if the directory cannot be created.
     */
    void prepareOutputDirectory() const;

  private:
    /**
     * Stores an already resolved output directory.
     * @param outputDirectory Directory used for generated session files.
     */
    explicit RuntimeConfiguration(std::filesystem::path outputDirectory);

    std::filesystem::path outputDirectory_;
};
