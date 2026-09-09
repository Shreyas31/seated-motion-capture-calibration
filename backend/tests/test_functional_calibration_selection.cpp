#include "frontend_protocol.h"
#include "functional_calibration_workflow.h"

#include <exception>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

void expect(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testAllChoicesResolveDefinitionsInOrder() {
    const std::vector<std::pair<std::size_t, std::string>> expectedChoices{
        {1, "Right knee"},
        {2, "Left knee"},
        {3, "Right elbow flexion/extension"},
        {4, "Right forearm pronation/supination"},
        {5, "Right shoulder abduction/adduction"},
        {6, "Left elbow flexion/extension"},
        {7, "Left forearm pronation/supination"},
        {8, "Left shoulder abduction/adduction"},
    };

    for (const auto& [choice, expectedName] : expectedChoices) {
        std::istringstream input{std::to_string(choice)};
        std::ostringstream output;
        std::ostringstream errors;
        frontend_protocol::EventWriter frontend{output};
        FunctionalCalibrationSelectionWorkflow workflow{frontend, input, output, errors};

        const auto selection = workflow.collect();
        expect(selection.has_value(), "Functional choice was rejected.");
        expect(selection->displayName == expectedName,
               "Functional choice resolved to the wrong definition.");
        expect(output.str().find("FRONTEND|STATE|JOINT") != std::string::npos,
               "Joint-selection state was not emitted.");
    }
}

void testOutOfRangeChoiceReturnsEmpty() {
    std::istringstream input{"9"};
    std::ostringstream output;
    std::ostringstream errors;
    frontend_protocol::EventWriter frontend{output};
    FunctionalCalibrationSelectionWorkflow workflow{frontend, input, output, errors};

    expect(!workflow.collect().has_value(), "Out-of-range functional choice was accepted.");
    expect(errors.str().find("Invalid joint choice") != std::string::npos,
           "Out-of-range choice did not report an error.");
}

void testNonNumericChoiceReturnsEmptyAndRecoversStream() {
    std::istringstream input{"invalid"};
    std::ostringstream output;
    std::ostringstream errors;
    frontend_protocol::EventWriter frontend{output};
    FunctionalCalibrationSelectionWorkflow workflow{frontend, input, output, errors};

    expect(!workflow.collect().has_value(), "Non-numeric functional choice was accepted.");
    expect(!input.fail(), "Input stream remained in a failed state.");
}

} // namespace

int main() {
    try {
        testAllChoicesResolveDefinitionsInOrder();
        testOutOfRangeChoiceReturnsEmpty();
        testNonNumericChoiceReturnsEmptyAndRecoversStream();
    } catch (const std::exception& error) {
        std::cerr << "Functional calibration selection test failed: " << error.what() << std::endl;
        return 1;
    }
    std::cout << "Functional calibration selection tests passed." << std::endl;
    return 0;
}
