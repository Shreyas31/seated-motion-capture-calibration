#include "runtime_configuration.h"

#include <chrono>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

namespace {

void expect(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testConfiguredDirectoryTakesPrecedence() {
    const RuntimeConfiguration configuration = RuntimeConfiguration::fromOutputDirectoryValue(
        std::string{"configured-output"}, "fallback-output");

    expect(configuration.outputDirectory() == "configured-output",
           "Configured output directory was ignored.");
}

void testMissingOrEmptyValueUsesFallback() {
    const std::filesystem::path fallback{"fallback-output"};

    const RuntimeConfiguration missing =
        RuntimeConfiguration::fromOutputDirectoryValue(std::nullopt, fallback);
    const RuntimeConfiguration empty =
        RuntimeConfiguration::fromOutputDirectoryValue(std::string{}, fallback);

    expect(missing.outputDirectory() == fallback, "Missing value did not use fallback.");
    expect(empty.outputDirectory() == fallback, "Empty value did not use fallback.");
}

void testPrepareCreatesOutputDirectory() {
    const std::filesystem::path outputDirectory =
        std::filesystem::temp_directory_path() /
        ("irp_seated_mocap_runtime_configuration_test_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

    const RuntimeConfiguration configuration =
        RuntimeConfiguration::fromOutputDirectoryValue(outputDirectory.string(), "unused-fallback");
    configuration.prepareOutputDirectory();

    expect(std::filesystem::is_directory(outputDirectory), "Output directory was not created.");

    std::error_code error;
    std::filesystem::remove_all(outputDirectory, error);
}

} // namespace

int main() {
    try {
        testConfiguredDirectoryTakesPrecedence();
        testMissingOrEmptyValueUsesFallback();
        testPrepareCreatesOutputDirectory();
    } catch (const std::exception& error) {
        std::cerr << "Runtime configuration test failed: " << error.what() << std::endl;
        return 1;
    }

    std::cout << "Runtime configuration tests passed." << std::endl;
    return 0;
}
