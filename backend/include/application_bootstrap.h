#pragma once

#include "sensor_mapping.h"
#include "session_setup_workflow.h"

#include <filesystem>
#include <iosfwd>
#include <memory>

class AwindaSystem;
namespace frontend_protocol {
class EventWriter;
}

/**
 * Owns values that must remain valid throughout one acquisition session.
 * @struct ApplicationBootstrapResult.
 */
struct ApplicationBootstrapResult {
    SensorMapping sensorMapping;
    SessionContext session;
    std::unique_ptr<AwindaSystem> awinda;
};

/**
 * Configures hardware, verifies sensors and heading, and creates the session context.
 * @class ApplicationBootstrap.
 */
class ApplicationBootstrap {
  public:
    /**
     * Creates a bootstrap workflow using injected protocol and console streams.
     * @param frontend Writer for structured frontend events.
     * @param input Stream used for clinician confirmation and session input.
     * @param output Stream used for normal status messages.
     * @param errorOutput Stream used for warnings and errors.
     */
    ApplicationBootstrap(frontend_protocol::EventWriter& frontend, std::istream& input,
                         std::ostream& output, std::ostream& errorOutput);

    /**
     * Prepares all resources required by the acquisition action loop.
     * @param sensorMappingPath Path to the sensor-assignment JSON file.
     * @return Mapping, session context, and configured measurement-mode Awinda system.
     * @throws std::exception if mapping, hardware, heading, sensor, or output setup fails.
     */
    ApplicationBootstrapResult prepare(const std::filesystem::path& sensorMappingPath) const;

  private:
    frontend_protocol::EventWriter& frontend_;
    std::istream& input_;
    std::ostream& output_;
    std::ostream& errorOutput_;
};
