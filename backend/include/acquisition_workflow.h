#pragma once

#include <iosfwd>

class AwindaSystem;
class SensorMapping;
struct SessionContext;
namespace frontend_protocol {
class EventWriter;
}

/**
 * Owns the runtime acquisition composition and clinician action loop.
 * @class AcquisitionWorkflow.
 */
class AcquisitionWorkflow {
  public:
    /**
     * Creates the action loop using injected protocol and console streams.
     * @param frontend Writer for structured frontend events.
     * @param input Stream used for clinician menu choices and stop commands.
     * @param output Stream used for normal status messages.
     * @param errorOutput Stream used for warnings and errors.
     */
    AcquisitionWorkflow(frontend_protocol::EventWriter& frontend, std::istream& input,
                        std::ostream& output, std::ostream& errorOutput);

    /**
     * Runs calibration and measurement actions until the clinician exits.
     * @param awinda Configured Awinda system already operating in measurement mode.
     * @param sensorMapping Validated 17-sensor anatomical mapping.
     * @param sessionContext Session name and output directory.
     * @return Zero after a normal clinician-requested exit.
     * @throws std::exception if callback registration or an unrecoverable workflow action fails.
     */
    int run(AwindaSystem& awinda, const SensorMapping& sensorMapping,
            const SessionContext& sessionContext) const;

  private:
    frontend_protocol::EventWriter& frontend_;
    std::istream& input_;
    std::ostream& output_;
    std::ostream& errorOutput_;
};
