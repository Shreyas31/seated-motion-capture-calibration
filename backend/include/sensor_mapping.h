#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

/**
 * Validated mapping between physical Xsens device IDs and the project's
 * canonical anatomical segment names.
 * @class SensorMapping.
 */
class SensorMapping {
  public:
    /**
     * Loads and validates a complete schema-version-1 sensor mapping.
     * @param filename JSON mapping file to read.
     * @return Validated bidirectional sensor mapping.
     * @throws std::runtime_error if the file cannot be read or its mapping is invalid.
     */
    static SensorMapping loadFromJson(const std::filesystem::path& filename);

    /**
     * Returns the ordered anatomical segment names required by the system.
     * @return Shared immutable list of all 17 canonical segment names.
     */
    static const std::vector<std::string>& canonicalSegments();

    /**
     * Returns every configured sensor-to-segment assignment.
     * @return Reference to the immutable mapping keyed by sensor ID.
     */
    const std::map<std::string, std::string>& sensorToSegment() const;

    /**
     * Finds the sensor assigned to an anatomical segment.
     * @param segment Canonical anatomical segment name.
     * @return Assigned sensor ID, or no value when the segment is unknown.
     */
    std::optional<std::string> sensorForSegment(const std::string& segment) const;

    /**
     * Finds the anatomical segment assigned to a sensor.
     * @param sensorId Physical Xsens device ID.
     * @return Canonical segment name, or no value when the sensor is unknown.
     */
    std::optional<std::string> segmentForSensor(const std::string& sensorId) const;

    /**
     * Returns the number of configured sensor assignments.
     * @return Number of sensor-to-segment entries.
     */
    std::size_t size() const;

  private:
    /**
     * Builds both lookup directions from an already validated mapping.
     * @param sensorToSegment Complete mapping keyed by physical sensor ID.
     */
    explicit SensorMapping(std::map<std::string, std::string> sensorToSegment);

    std::map<std::string, std::string> sensorToSegment_;
    std::map<std::string, std::string> segmentToSensor_;
};
