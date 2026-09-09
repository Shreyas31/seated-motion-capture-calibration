#include "application_bootstrap.h"

#include "awinda_system.h"
#include "frontend_protocol.h"
#include "heading_offset_workflow.h"
#include "runtime_configuration.h"
#include "sensor_connection_workflow.h"

#include <memory>
#include <ostream>
#include <string>
#include <utility>

namespace {

constexpr int DESIRED_UPDATE_RATE_HZ = 60;
constexpr int RADIO_CHANNEL = 19;

} // namespace

ApplicationBootstrap::ApplicationBootstrap(frontend_protocol::EventWriter& frontend,
                                           std::istream& input, std::ostream& output,
                                           std::ostream& errorOutput)
    : frontend_{frontend}, input_{input}, output_{output}, errorOutput_{errorOutput} {}

ApplicationBootstrapResult
ApplicationBootstrap::prepare(const std::filesystem::path& sensorMappingPath) const {

    SensorMapping sensorMapping = SensorMapping::loadFromJson(sensorMappingPath);
    const auto& sensorToSegment = sensorMapping.sensorToSegment();

    output_ << "\nLoaded sensor mapping from:\n  " << sensorMappingPath.string()
            << "\n\nConfigured sensor assignments:" << std::endl;
    for (const std::string& segment : SensorMapping::canonicalSegments()) {
        output_ << "  " << segment << " -> "
                << sensorMapping.sensorForSegment(segment).value_or("<unassigned>") << std::endl;
    }

    output_ << "Starting Awinda Mocap System..." << std::endl;
    auto awinda = std::make_unique<AwindaSystem>();
    awinda->configure(DESIRED_UPDATE_RATE_HZ, RADIO_CHANNEL);

    SensorConnectionWorkflow sensorConnections{frontend_, input_, output_};
    sensorConnections.waitForConfirmation(*awinda, sensorToSegment);

    // Heading configuration must be resolved while the master remains in
    // Config mode. It cannot safely change during acquisition.
    awinda->refreshConnectedDevices();
    xsens_heading::HeadingOffsetService headingOffsets{output_, errorOutput_};
    xsens_heading::HeadingOffsetWorkflow headingWorkflow{headingOffsets, frontend_, input_, output_,
                                                         errorOutput_};
    headingWorkflow.verifyAndResolve(awinda->devices());
    awinda->enterMeasurementMode();

    const RuntimeConfiguration runtimeConfiguration = RuntimeConfiguration::fromEnvironment();
    SessionSetupWorkflow sessionSetup{frontend_, input_, output_};
    SessionContext session = sessionSetup.prepare(runtimeConfiguration);

    return ApplicationBootstrapResult{std::move(sensorMapping), std::move(session),
                                      std::move(awinda)};
}
