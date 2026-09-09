#include "application_menu.h"
#include "frontend_protocol.h"

#include <exception>
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

void testAllNumericChoicesMapToActions() {
    expect(applicationActionFromChoice(1) == ApplicationAction::StaticChair,
           "Chair choice changed.");
    expect(applicationActionFromChoice(2) == ApplicationAction::StaticBed, "Bed choice changed.");
    expect(applicationActionFromChoice(3) == ApplicationAction::FunctionalCalibration,
           "Functional choice changed.");
    expect(applicationActionFromChoice(4) == ApplicationAction::RecordMeasurement,
           "Recording choice changed.");
    expect(applicationActionFromChoice(5) == ApplicationAction::Exit, "Exit choice changed.");
    expect(!applicationActionFromChoice(0) && !applicationActionFromChoice(6),
           "Out-of-range menu choice was accepted.");
}

void testStaticActionsMapToPoseChoices() {
    expect(staticPoseChoice(ApplicationAction::StaticChair) == 1,
           "Chair action did not map to pose 1.");
    expect(staticPoseChoice(ApplicationAction::StaticBed) == 2,
           "Bed action did not map to pose 2.");
    expect(!staticPoseChoice(ApplicationAction::RecordMeasurement),
           "Non-static action mapped to a pose.");
}

void testMalformedInputRecoversAndEmitsMenuState() {
    std::istringstream input{"invalid\n4"};
    std::ostringstream output;
    frontend_protocol::EventWriter frontend{output};
    ApplicationMenu menu{frontend, input, output};

    expect(!menu.collect(), "Malformed menu input was accepted.");
    expect(menu.collect() == ApplicationAction::RecordMeasurement,
           "Menu did not recover after malformed input.");
    expect(output.str().find("FRONTEND|STATE|MENU") != std::string::npos,
           "Menu state was not emitted.");
}

void testEndOfInputRequestsExit() {
    std::istringstream input;
    std::ostringstream output;
    frontend_protocol::EventWriter frontend{output};
    ApplicationMenu menu{frontend, input, output};
    expect(menu.collect() == ApplicationAction::Exit, "End-of-input did not request a clean exit.");
}

} // namespace

int main() {
    try {
        testAllNumericChoicesMapToActions();
        testStaticActionsMapToPoseChoices();
        testMalformedInputRecoversAndEmitsMenuState();
        testEndOfInputRequestsExit();
    } catch (const std::exception& error) {
        std::cerr << "Application menu test failed: " << error.what() << std::endl;
        return 1;
    }
    std::cout << "Application menu tests passed." << std::endl;
    return 0;
}
