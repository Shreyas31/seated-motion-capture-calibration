#include "runtime_configuration.h"

#include <cstdlib>
#include <utility>

namespace {

std::optional<std::string> readEnvironmentVariable(const char* name) {

#ifdef _WIN32
    char* value = nullptr;
    std::size_t valueLength = 0;

    if (_dupenv_s(&value, &valueLength, name) != 0 || value == nullptr) {
        return std::nullopt;
    }

    const std::string result{value};
    std::free(value);
    return result;
#else
    const char* value = std::getenv(name);

    if (value == nullptr) {
        return std::nullopt;
    }

    return std::string{value};
#endif
}

} // namespace

RuntimeConfiguration::RuntimeConfiguration(std::filesystem::path outputDirectory)
    : outputDirectory_{std::move(outputDirectory)} {}

RuntimeConfiguration
RuntimeConfiguration::fromEnvironment(const std::filesystem::path& fallbackDirectory) {

    return fromOutputDirectoryValue(readEnvironmentVariable("SEATED_MOCAP_OUTPUT_DIR"),
                                    fallbackDirectory);
}

RuntimeConfiguration RuntimeConfiguration::fromOutputDirectoryValue(
    const std::optional<std::string>& configuredDirectory,
    const std::filesystem::path& fallbackDirectory) {

    if (configuredDirectory && !configuredDirectory->empty()) {
        return RuntimeConfiguration{std::filesystem::path{*configuredDirectory}};
    }

    return RuntimeConfiguration{fallbackDirectory};
}

const std::filesystem::path& RuntimeConfiguration::outputDirectory() const noexcept {
    return outputDirectory_;
}

void RuntimeConfiguration::prepareOutputDirectory() const {
    std::filesystem::create_directories(outputDirectory_);
}
