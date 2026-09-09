#include "realtime_viewer_configuration.h"

#include <cassert>
#include <stdexcept>

namespace {

template <typename Function> void expectRuntimeError(Function&& function) {
    bool threw = false;
    try {
        function();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);
}

void testRequiredArgumentsUseDefaults() {
    const char* arguments[]{"opensim_rt_viewer", "model.osim", "presets.json", "chair", "Geometry"};
    const auto configuration = SeatedMoCap::parseRealtimeViewerConfiguration(5, arguments);

    assert(configuration.modelPath == "model.osim");
    assert(configuration.presetPath == "presets.json");
    assert(configuration.poseName == "chair");
    assert(configuration.geometryPath == "Geometry");
    assert(configuration.listeningDurationSeconds == 120);
    assert(!configuration.stationaryFeetEnabled());
    assert(configuration.jointAngleOutputPath.empty());
}

void testAllOptionalArgumentsAreParsedTogether() {
    const char* arguments[]{"opensim_rt_viewer", "model.osim", "presets.json",    "chair",
                            "Geometry",          "45",         "stationary-feet", "angles.csv"};
    const auto configuration = SeatedMoCap::parseRealtimeViewerConfiguration(8, arguments);

    assert(configuration.listeningDurationSeconds == 45);
    assert(configuration.stationaryFeetEnabled());
    assert(configuration.jointAngleOutputPath == "angles.csv");
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
    assert(!SeatedMoCap::isRealtimeViewerArgumentCountValid(4));
    assert(SeatedMoCap::isRealtimeViewerArgumentCountValid(5));
    assert(SeatedMoCap::isRealtimeViewerArgumentCountValid(8));
    assert(!SeatedMoCap::isRealtimeViewerArgumentCountValid(9));

    testRequiredArgumentsUseDefaults();
    testAllOptionalArgumentsAreParsedTogether();
    testInvalidDurationIsRejected();
    testInvalidTranslationModeIsRejected();
    return 0;
}
