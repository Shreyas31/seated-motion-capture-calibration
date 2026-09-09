#include "sensor_connection_workflow.h"

#include <exception>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

namespace {

void expect(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

const std::map<std::string, std::string> SENSOR_MAP = {
    {"ID-A", "Pelvis"}, {"ID-B", "Sternum"}, {"ID-C", "Head"}};

void testCompleteMappedSet() {
    const auto summary = analyzeSensorConnections({"ID-A", "ID-B", "ID-C"}, SENSOR_MAP);
    expect(summary.mappedConnectedCount == 3, "Mapped count was wrong.");
    expect(summary.complete(SENSOR_MAP.size()), "Complete set was rejected.");
    expect(summary.unmappedSensorIds.empty(), "Unexpected unmapped sensor.");
}

void testMissingMappedSensorIncludesSegmentAndId() {
    const auto summary = analyzeSensorConnections({"ID-A", "ID-C"}, SENSOR_MAP);
    expect(!summary.complete(SENSOR_MAP.size()), "Incomplete set passed.");
    expect(summary.missingDescription() == "Sternum (ID-B)", "Missing sensor description changed.");
}

void testUnmappedSensorIsSeparate() {
    const auto summary = analyzeSensorConnections({"ID-A", "ID-B", "ID-C", "ID-X"}, SENSOR_MAP);
    expect(summary.complete(SENSOR_MAP.size()), "Mapped set was incomplete.");
    expect(summary.unmappedSensorIds == std::vector<std::string>{"ID-X"},
           "Unmapped sensor was not reported.");
}

} // namespace

int main() {
    try {
        testCompleteMappedSet();
        testMissingMappedSensorIncludesSegmentAndId();
        testUnmappedSensorIsSeparate();
    } catch (const std::exception& error) {
        std::cerr << "Sensor connection status test failed: " << error.what() << std::endl;
        return 1;
    }
    std::cout << "Sensor connection status tests passed." << std::endl;
    return 0;
}
