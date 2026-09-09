#pragma once

#include <chrono>
#include <cstddef>
#include <iosfwd>
#include <map>
#include <set>
#include <string>
#include <vector>

class AwindaSystem;
namespace frontend_protocol {
class EventWriter;
}

/**
 * Summarises mapped, missing, and unrecognised sensors at one discovery instant.
 * @struct SensorConnectionSummary.
 */
struct SensorConnectionSummary {
    std::size_t mappedConnectedCount = 0;
    std::vector<std::string> unmappedSensorIds;
    std::vector<std::string> missingMappedSensors;

    /**
     * Checks whether every required mapped sensor is connected.
     * @param requiredCount Number of mapped sensors required by the configuration.
     * @return True when the connected mapped count is complete and none are missing.
     */
    bool complete(std::size_t requiredCount) const noexcept;

    /**
     * Joins missing segment and sensor labels for clinician-facing diagnostics.
     * @return Comma-separated missing-sensor description.
     */
    std::string missingDescription() const;
};

/**
 * Compares currently connected sensor IDs with the configured assignments.
 * @param connectedSensorIds Physical device IDs currently reported by Awinda.
 * @param sensorToSegment Expected mapping from physical sensor ID to segment name.
 * @return Connection summary containing mapped, missing, and unmapped sensors.
 */
SensorConnectionSummary
analyzeSensorConnections(const std::set<std::string>& connectedSensorIds,
                         const std::map<std::string, std::string>& sensorToSegment);

/**
 * Polls sensor discovery, reports live status, and waits for clinician confirmation.
 * @class SensorConnectionWorkflow.
 */
class SensorConnectionWorkflow {
  public:
    /**
     * Creates a sensor-confirmation workflow.
     * @param frontend Writer for live sensor-status events.
     * @param input Stream on which ENTER confirms the displayed sensor set.
     * @param output Stream used for clinician instructions.
     * @param pollInterval Interval between hardware discovery checks.
     */
    SensorConnectionWorkflow(frontend_protocol::EventWriter& frontend, std::istream& input,
                             std::ostream& output,
                             std::chrono::milliseconds pollInterval = std::chrono::milliseconds{
                                 250});

    /**
     * Blocks until confirmation, then rejects an incomplete mapped sensor set.
     * @param awinda Configured Awinda system in discovery/configuration mode.
     * @param sensorToSegment Complete expected sensor assignment mapping.
     * @return Final confirmed connection summary.
     * @throws std::runtime_error if any required mapped sensor is disconnected.
     */
    SensorConnectionSummary
    waitForConfirmation(const AwindaSystem& awinda,
                        const std::map<std::string, std::string>& sensorToSegment) const;

  private:
    /**
     * Emits per-sensor and aggregate connection status to the frontend.
     * @param connectedSensorIds Physical IDs currently connected.
     * @param sensorToSegment Expected sensor assignment mapping.
     * @param summary Analysis of the same discovery snapshot.
     */
    void reportStatus(const std::set<std::string>& connectedSensorIds,
                      const std::map<std::string, std::string>& sensorToSegment,
                      const SensorConnectionSummary& summary) const;

    frontend_protocol::EventWriter& frontend_;
    std::istream& input_;
    std::ostream& output_;
    std::chrono::milliseconds pollInterval_;
};
