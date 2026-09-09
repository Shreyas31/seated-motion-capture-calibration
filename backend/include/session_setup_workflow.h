#pragma once

#include <filesystem>
#include <iosfwd>
#include <string>

class RuntimeConfiguration;
namespace frontend_protocol {
class EventWriter;
}

/**
 * Stores the sanitised session name and its session-specific output directory.
 * @struct SessionContext.
 */
struct SessionContext {
    std::string name;
    std::filesystem::path outputDirectory;
};

/**
 * Collects the clinician's session identifier and prepares its output location.
 * @class SessionSetupWorkflow.
 */
class SessionSetupWorkflow {
  public:
    /**
     * Creates a session setup workflow using injected protocol and console streams.
     * @param frontend Writer for structured frontend events.
     * @param input Stream used to read the session identifier.
     * @param output Stream used for prompts and resolved-path messages.
     */
    SessionSetupWorkflow(frontend_protocol::EventWriter& frontend, std::istream& input,
                         std::ostream& output);

    /**
     * Sanitises the entered name and creates its subdirectory beneath the
     * configured output root.
     * @param configuration Runtime configuration providing the output directory.
     * @return Prepared session name and output path.
     * @throws std::filesystem::filesystem_error if the output directory cannot be created.
     */
    SessionContext prepare(const RuntimeConfiguration& configuration) const;

  private:
    frontend_protocol::EventWriter& frontend_;
    std::istream& input_;
    std::ostream& output_;
};
