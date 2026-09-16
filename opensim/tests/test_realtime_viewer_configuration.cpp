#include "realtime_viewer_configuration.h"

#include <stdexcept>
#include <string>

namespace {

void expect(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename Function> void expectRuntimeError(Function&& function) {
    bool threw = false;
    try {
        function();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    expect(threw, "Invalid viewer arguments did not raise a runtime error.");
}

void testRequiredArgumentsUseDefaults() {
    const char* arguments[]{"opensim_rt_viewer", "model.osim", "presets.json", "chair", "Geometry"};
    const auto configuration = SeatedMoCap::parseRealtimeViewerConfiguration(5, arguments);

    expect(configuration.modelPath == "model.osim", "The model path was parsed incorrectly.");
    expect(configuration.presetPath == "presets.json", "The preset path was parsed incorrectly.");
    expect(configuration.poseName == "chair", "The pose name was parsed incorrectly.");
    expect(configuration.geometryPath == "Geometry", "The geometry path was parsed incorrectly.");
    expect(configuration.listeningDurationSeconds == 120,
           "The default listening duration was incorrect.");
    expect(!configuration.stationaryFeetEnabled(),
           "Stationary feet were unexpectedly enabled by default.");
    expect(configuration.jointAngleOutputPath.empty(),
           "A joint-angle output path was unexpectedly set by default.");
}

void testAllOptionalArgumentsAreParsedTogether() {
    const char* arguments[]{"opensim_rt_viewer", "model.osim", "presets.json",    "chair",
                            "Geometry",          "45",         "stationary-feet", "angles.csv"};
    const auto configuration = SeatedMoCap::parseRealtimeViewerConfiguration(8, arguments);

    expect(configuration.listeningDurationSeconds == 45,
           "The optional listening duration was parsed incorrectly.");
    expect(configuration.stationaryFeetEnabled(), "The stationary-feet option was not enabled.");
    expect(configuration.jointAngleOutputPath == "angles.csv",
           "The joint-angle output path was parsed incorrectly.");
}

void testInvalidDurationIsRejected() {
    const char* zero[]{"opensim_rt_viewer", "model.osim", "presets.json", "chair", "Geometry", "0"};
    const char* partial[]{"opensim_rt_viewer", "model.osim", "presets.json", "chair",
                          "Geometry",          "12seconds"};

    expectRuntimeError([&] {
        const auto configuration = SeatedMoCap::parseRealtimeViewerConfiguration(6, zero);
        (void)configuration;
    });
    expectRuntimeError([&] {
        const auto configuration = SeatedMoCap::parseRealtimeViewerConfiguration(6, partial);
        (void)configuration;
    });
}

void testInvalidTranslationModeIsRejected() {
    const char* arguments[]{"opensim_rt_viewer", "model.osim", "presets.json", "chair",
                            "Geometry",          "45",         "fixed-pelvis"};
    expectRuntimeError([&] {
        const auto configuration = SeatedMoCap::parseRealtimeViewerConfiguration(7, arguments);
        (void)configuration;
    });
}

} // namespace

int main() {
    expect(!SeatedMoCap::isRealtimeViewerArgumentCountValid(4),
           "Too few viewer arguments were accepted.");
    expect(SeatedMoCap::isRealtimeViewerArgumentCountValid(5),
           "The required viewer arguments were rejected.");
    expect(SeatedMoCap::isRealtimeViewerArgumentCountValid(8),
           "The full viewer argument list was rejected.");
    expect(!SeatedMoCap::isRealtimeViewerArgumentCountValid(9),
           "Too many viewer arguments were accepted.");

    testRequiredArgumentsUseDefaults();
    testAllOptionalArgumentsAreParsedTogether();
    testInvalidDurationIsRejected();
    testInvalidTranslationModeIsRejected();
    return 0;
}
