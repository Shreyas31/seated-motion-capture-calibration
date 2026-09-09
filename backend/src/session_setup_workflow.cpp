#include "session_setup_workflow.h"

#include "frontend_protocol.h"
#include "output_naming.h"
#include "runtime_configuration.h"

#include <istream>
#include <ostream>

SessionSetupWorkflow::SessionSetupWorkflow(frontend_protocol::EventWriter& frontend,
                                           std::istream& input, std::ostream& output)
    : frontend_{frontend}, input_{input}, output_{output} {}

SessionContext SessionSetupWorkflow::prepare(const RuntimeConfiguration& configuration) const {

    frontend_.emitState(frontend_protocol::state::kSession);
    output_ << "Enter a session/file prefix (for example P01_20260726): ";

    std::string enteredName;
    std::getline(input_ >> std::ws, enteredName);
    const std::string sessionName = output_naming::safeFilenamePart(enteredName);

    configuration.prepareOutputDirectory();

    const std::filesystem::path sessionDirectory = configuration.outputDirectory() / sessionName;
    std::filesystem::create_directories(sessionDirectory);

    SessionContext context{sessionName, sessionDirectory};
    frontend_.emitResult(frontend_protocol::result::kSessionConfigured, sessionName);

    output_ << "Session output directory:\n" << context.outputDirectory.string() << std::endl;

    return context;
}
