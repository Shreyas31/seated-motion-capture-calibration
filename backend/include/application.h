#pragma once

#include <filesystem>

/**
 * Coordinates application startup and the complete acquisition workflow.
 * @class Application.
 */
class Application {
  public:
    /**
     * Starts hardware setup and runs the clinician interaction loop.
     * @param sensorMappingPath Path to the validated sensor-assignment JSON file.
     * @return Process exit code returned by the acquisition workflow.
     * @throws std::exception if startup, hardware configuration, or session setup fails.
     */
    int run(const std::filesystem::path& sensorMappingPath);
};
