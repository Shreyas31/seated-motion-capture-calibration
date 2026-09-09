#include "sensor_connection_workflow.h"

#include "awinda_system.h"
#include "frontend_protocol.h"

#include <atomic>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <thread>

bool SensorConnectionSummary::complete(const std::size_t requiredCount) const noexcept {
    return mappedConnectedCount == requiredCount && missingMappedSensors.empty();
}

std::string SensorConnectionSummary::missingDescription() const {
    std::string description;
    for (const std::string& missingSensor : missingMappedSensors) {
        if (!description.empty()) {
            description += ", ";
        }
        description += missingSensor;
    }
    return description;
}

SensorConnectionSummary
analyzeSensorConnections(const std::set<std::string>& connectedSensorIds,
                         const std::map<std::string, std::string>& sensorToSegment) {

    SensorConnectionSummary summary;
    for (const auto& [sensorId, segment] : sensorToSegment) {
        if (connectedSensorIds.contains(sensorId)) {
            ++summary.mappedConnectedCount;
        } else {
            summary.missingMappedSensors.push_back(segment + " (" + sensorId + ")");
        }
    }

    for (const std::string& sensorId : connectedSensorIds) {
        if (!sensorToSegment.contains(sensorId)) {
            summary.unmappedSensorIds.push_back(sensorId);
        }
    }
    return summary;
}

SensorConnectionWorkflow::SensorConnectionWorkflow(frontend_protocol::EventWriter& frontend,
                                                   std::istream& input, std::ostream& output,
                                                   const std::chrono::milliseconds pollInterval)
    : frontend_{frontend}, input_{input}, output_{output}, pollInterval_{pollInterval} {}

void SensorConnectionWorkflow::reportStatus(
    const std::set<std::string>& connectedSensorIds,
    const std::map<std::string, std::string>& sensorToSegment,
    const SensorConnectionSummary& summary) const {

    for (const auto& [sensorId, segment] : sensorToSegment) {
        (void)segment;
        frontend_.emitResult(
            frontend_protocol::result::kSensorStatus,
            sensorId + ":" +
                (connectedSensorIds.contains(sensorId) ? "CONNECTED" : "DISCONNECTED"));
    }
    for (const std::string& sensorId : summary.unmappedSensorIds) {
        frontend_.emitResult(frontend_protocol::result::kUnmappedSensor, sensorId);
    }
    frontend_.emitResult(frontend_protocol::result::kSensorsConnected,
                         std::to_string(summary.mappedConnectedCount));
}

SensorConnectionSummary SensorConnectionWorkflow::waitForConfirmation(
    const AwindaSystem& awinda, const std::map<std::string, std::string>& sensorToSegment) const {

    frontend_.emitState(frontend_protocol::state::kSensors);
    output_ << "Turn on patient MTw sensors now." << std::endl;
    output_ << "Press ENTER when all sensors are connected "
            << "to begin recording..." << std::endl;

    std::atomic<bool> sensorsConfirmed{false};
    std::thread confirmationThread([this, &sensorsConfirmed]() {
        input_.get();
        sensorsConfirmed.store(true);
    });

    std::set<std::string> lastReportedSensorIds;
    bool firstReport = true;
    while (!sensorsConfirmed.load()) {
        const auto currentSensorIds = awinda.connectedSensorIds();
        if (firstReport || currentSensorIds != lastReportedSensorIds) {
            const auto summary = analyzeSensorConnections(currentSensorIds, sensorToSegment);
            reportStatus(currentSensorIds, sensorToSegment, summary);
            lastReportedSensorIds = currentSensorIds;
            firstReport = false;
        }
        std::this_thread::sleep_for(pollInterval_);
    }
    confirmationThread.join();

    const auto finalConnectedSensorIds = awinda.connectedSensorIds();
    const auto finalSummary = analyzeSensorConnections(finalConnectedSensorIds, sensorToSegment);
    reportStatus(finalConnectedSensorIds, sensorToSegment, finalSummary);
    output_ << "Proceeding with " << finalSummary.mappedConnectedCount << " active sensors."
            << std::endl;

    if (!finalSummary.complete(sensorToSegment.size())) {
        const std::string missingSensors = finalSummary.missingDescription();
        frontend_.emitError(frontend_protocol::error::kSensorsIncomplete, missingSensors);
        throw std::runtime_error("Required mapped sensors are disconnected: " + missingSensors);
    }
    return finalSummary;
}
