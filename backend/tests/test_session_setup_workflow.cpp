#include "frontend_protocol.h"
#include "runtime_configuration.h"
#include "session_setup_workflow.h"

#include <chrono>
#include <exception>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

void expect(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testPrepareSanitizesNameAndCreatesDirectory() {
    const std::filesystem::path outputDirectory =
        std::filesystem::temp_directory_path() /
        ("irp_session_setup_test_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

    std::istringstream input{"Patient 01 / chair\n"};
    std::ostringstream output;
    frontend_protocol::EventWriter frontend{output};
    SessionSetupWorkflow workflow{frontend, input, output};
    const RuntimeConfiguration configuration =
        RuntimeConfiguration::fromOutputDirectoryValue(outputDirectory.string(), "unused-fallback");

    const SessionContext context = workflow.prepare(configuration);
    const std::filesystem::path expectedSessionDirectory = outputDirectory / "Patient_01_chair";

    expect(context.name == "Patient_01_chair", "Session name was not sanitized consistently.");
    expect(context.outputDirectory == expectedSessionDirectory,
           "Session-specific output directory was not selected.");
    expect(std::filesystem::is_directory(expectedSessionDirectory),
           "Session-specific output directory was not created.");
    expect(output.str().find("FRONTEND|STATE|SESSION") != std::string::npos,
           "Session state was not emitted.");
    expect(output.str().find("FRONTEND|RESULT|SESSION_CONFIGURED|Patient_01_chair") !=
               std::string::npos,
           "Sanitized session result was not emitted.");

    std::error_code error;
    std::filesystem::remove_all(outputDirectory, error);
}

void testEmptyNameUsesDefault() {
    std::istringstream input{"   \n"};
    std::ostringstream output;
    frontend_protocol::EventWriter frontend{output};
    SessionSetupWorkflow workflow{frontend, input, output};
    const RuntimeConfiguration configuration = RuntimeConfiguration::fromOutputDirectoryValue(
        std::filesystem::temp_directory_path().string(), "unused-fallback");

    const SessionContext context = workflow.prepare(configuration);
    expect(context.name == "Session", "Empty session input did not use the default name.");
}

} // namespace

int main() {
    try {
        testPrepareSanitizesNameAndCreatesDirectory();
        testEmptyNameUsesDefault();
    } catch (const std::exception& error) {
        std::cerr << "Session setup workflow test failed: " << error.what() << std::endl;
        return 1;
    }
    std::cout << "Session setup workflow tests passed." << std::endl;
    return 0;
}
