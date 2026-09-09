#include "application.h"

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#ifndef SEATED_MOCAP_DEFAULT_SENSOR_MAP
#define SEATED_MOCAP_DEFAULT_SENSOR_MAP "config/sensor_mapping.json"
#endif

namespace {

std::filesystem::path parseSensorMappingPath(const int argc, char* argv[]) {

    std::filesystem::path result = SEATED_MOCAP_DEFAULT_SENSOR_MAP;

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];

        if (argument == "--sensor-map") {
            if (index + 1 >= argc) {
                throw std::runtime_error("--sensor-map requires a file path.");
            }

            result = argv[++index];
        } else if (argument == "--help" || argument == "-h") {
            std::cout << "Usage:\n"
                      << "  backend.exe "
                      << "[--sensor-map <sensor_mapping.json>]\n";

            std::exit(0);
        } else {
            throw std::runtime_error("Unknown command-line argument: " + argument);
        }
    }

    return result;
}

} // namespace

int main(int argc, char* argv[]) {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    try {
        const std::filesystem::path sensorMappingPath = parseSensorMappingPath(argc, argv);

        Application application;

        return application.run(sensorMappingPath);
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << '\n';

        return 1;
    } catch (...) {
        std::cerr << "Fatal error: unknown exception.\n";

        return 1;
    }
}
