#include "frontend_protocol.h"
#include "realtime_orientation_packet.h"

#include <array>
#include <exception>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

using namespace std::string_view_literals;

void expect(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testFormatWithoutDetail() {
    expect(frontend_protocol::EventWriter::formatEvent(frontend_protocol::category::kState,
                                                       frontend_protocol::state::kSensors) ==
               "FRONTEND|STATE|SENSORS",
           "State event format changed.");
}

void testFormatPreservesDetailPipes() {
    expect(frontend_protocol::EventWriter::formatEvent(
               frontend_protocol::category::kError,
               frontend_protocol::error::kFunctionalCalibration,
               "Right knee|insufficient movement") == "FRONTEND|ERROR|FUNCTIONAL_CALIBRATION|"
                                                      "Right knee|insufficient movement",
           "Event detail was not preserved exactly.");
}

void testWriterUsesExistingLineFraming() {
    std::ostringstream output;
    const frontend_protocol::EventWriter writer{output};

    const std::string sensorCount = std::to_string(SeatedMoCap::Realtime::kSensorCount);
    writer.emitResult(frontend_protocol::result::kSensorsConnected, sensorCount);

    expect(output.str() == "\nFRONTEND|RESULT|SENSORS_CONNECTED|" + sensorCount + "\n",
           "Writer line framing changed.");
}

void testStableVocabulary() {
    using namespace frontend_protocol;

    constexpr std::array expectedCategories{"STATE"sv, "RESULT"sv, "ERROR"sv, "GUIDANCE"sv};
    constexpr std::array expectedStates{"SENSORS"sv, "HEADING"sv,   "SESSION"sv, "MENU"sv,
                                        "JOINT"sv,   "RECORDING"sv, "EXITING"sv};
    constexpr std::array expectedResults{"SENSOR_STATUS"sv,      "UNMAPPED_SENSOR"sv,
                                         "SENSORS_CONNECTED"sv,  "SESSION_CONFIGURED"sv,
                                         "STATIC_CALIBRATION"sv, "FUNCTIONAL_CALIBRATION"sv,
                                         "RECORDING_STARTED"sv,  "RECORDING_STOPPED"sv};
    constexpr std::array expectedErrors{"SENSORS_INCOMPLETE"sv, "STATIC_CALIBRATION"sv,
                                        "FUNCTIONAL_CALIBRATION"sv, "RECORDING"sv};
    constexpr std::array expectedGuidance{"STATIC_PREPARATION"sv,
                                          "STATIC_CAPTURE"sv,
                                          "STATIC_PROCESSING"sv,
                                          "FUNCTIONAL_GRAVITY_PREPARATION"sv,
                                          "FUNCTIONAL_GRAVITY_CAPTURE"sv,
                                          "FUNCTIONAL_GRAVITY_PROCESSING"sv,
                                          "FUNCTIONAL_MOVEMENT_PREPARATION"sv,
                                          "FUNCTIONAL_MOVEMENT_CAPTURE"sv,
                                          "FUNCTIONAL_RETURN_POSE_PREPARATION"sv,
                                          "FUNCTIONAL_RETURN_POSE_CAPTURE"sv,
                                          "FUNCTIONAL_PROCESSING"sv,
                                          "COUNTDOWN"sv};

    expect(category::kAll == expectedCategories, "Protocol categories changed.");
    expect(state::kAll == expectedStates, "Protocol state names changed.");
    expect(result::kAll == expectedResults, "Protocol result names changed.");
    expect(error::kAll == expectedErrors, "Protocol error names changed.");
    expect(guidance::kAll == expectedGuidance, "Protocol guidance names changed.");
}

} // namespace

int main() {
    try {
        testFormatWithoutDetail();
        testFormatPreservesDetailPipes();
        testWriterUsesExistingLineFraming();
        testStableVocabulary();
    } catch (const std::exception& error) {
        std::cerr << "Frontend protocol test failed: " << error.what() << std::endl;
        return 1;
    }

    std::cout << "Frontend protocol tests passed." << std::endl;
    return 0;
}
