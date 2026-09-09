#include "frontend_protocol.h"
#include "static_calibration_workflow.h"

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

void testAllPoseLabels() {
    const auto chair = buildStaticPoseSelection(1);
    const auto bed = buildStaticPoseSelection(2);

    expect(chair.pose == PoseType::Chair, "Chair choice mapped to the wrong pose.");
    expect(chair.description == "90-degree-leg chair with palms facing each other",
           "Chair description changed.");
    expect(chair.fileLabel == "Chair_PalmsTogether", "Chair filename label changed.");
    expect(bed.description == "straight-leg bed with palms facing each other",
           "Bed description changed.");
    expect(bed.fileLabel == "Bed_PalmsTogether", "Bed filename label changed.");
}

void testInvalidPoseChoiceIsRejected() {
    bool threw = false;
    try {
        (void)buildStaticPoseSelection(3);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    expect(threw, "Invalid static pose choice was accepted.");
}

} // namespace

int main() {
    try {
        testAllPoseLabels();
        testInvalidPoseChoiceIsRejected();
    } catch (const std::exception& error) {
        std::cerr << "Static pose selection test failed: " << error.what() << std::endl;
        return 1;
    }
    std::cout << "Static pose selection tests passed." << std::endl;
    return 0;
}
