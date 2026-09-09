#include "measurement_recording_workflow.h"

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

void testZeroStopsImmediately() {
    std::istringstream input{"0 7"};
    waitForRecordingStop(input);
    int remaining = 0;
    input >> remaining;
    expect(remaining == 7, "Stop parser consumed input after zero.");
}

void testNonzeroAndMalformedInputsAreIgnored() {
    std::istringstream input{"4\ninvalid\n-2\n0\n9"};
    waitForRecordingStop(input);
    int remaining = 0;
    input >> remaining;
    expect(remaining == 9, "Stop parser did not recover from malformed/non-zero input.");
}

void testEndOfInputReturns() {
    std::istringstream input;
    waitForRecordingStop(input);
    expect(input.eof(), "Empty input did not terminate at EOF.");
}

} // namespace

int main() {
    try {
        testZeroStopsImmediately();
        testNonzeroAndMalformedInputsAreIgnored();
        testEndOfInputReturns();
    } catch (const std::exception& error) {
        std::cerr << "Recording stop-input test failed: " << error.what() << std::endl;
        return 1;
    }
    std::cout << "Recording stop-input tests passed." << std::endl;
    return 0;
}
