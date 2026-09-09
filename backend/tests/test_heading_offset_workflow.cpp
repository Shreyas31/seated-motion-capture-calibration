#include "frontend_protocol.h"
#include "heading_offset_workflow.h"

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

void testResetChoiceParsing() {
    using xsens_heading::HeadingResetDecision;
    using xsens_heading::parseHeadingResetDecision;

    expect(parseHeadingResetDecision('y') == HeadingResetDecision::Reset,
           "Lowercase reset choice was rejected.");
    expect(parseHeadingResetDecision('Y') == HeadingResetDecision::Reset,
           "Uppercase reset choice was rejected.");
    expect(parseHeadingResetDecision('n') == HeadingResetDecision::Continue,
           "Default continuation choice was rejected.");
    expect(parseHeadingResetDecision('x') == HeadingResetDecision::Continue,
           "Unexpected input did not use the safe default.");
}

void testDeclinedResetContinuesWithWarning() {
    std::istringstream input{"n"};
    std::ostringstream output;
    std::ostringstream errors;
    frontend_protocol::EventWriter frontend{output};
    xsens_heading::HeadingOffsetService service{output, errors};
    xsens_heading::HeadingOffsetWorkflow workflow{service, frontend, input, output, errors};

    workflow.verifyAndResolve({});

    expect(output.str().find("FRONTEND|STATE|HEADING") != std::string::npos,
           "Heading state was not emitted.");
    expect(errors.str().find("Continuing without changing") != std::string::npos,
           "Declined reset did not produce a warning.");
}

void testAcceptedResetReportsSuccess() {
    std::istringstream input{"Y"};
    std::ostringstream output;
    std::ostringstream errors;
    frontend_protocol::EventWriter frontend{output};
    xsens_heading::HeadingOffsetService service{output, errors};
    xsens_heading::HeadingOffsetWorkflow workflow{service, frontend, input, output, errors};

    workflow.verifyAndResolve({});

    expect(output.str().find("reset and verified at zero") != std::string::npos,
           "Accepted reset did not report success.");
}

} // namespace

int main() {
    try {
        testResetChoiceParsing();
        testDeclinedResetContinuesWithWarning();
        testAcceptedResetReportsSuccess();
    } catch (const std::exception& error) {
        std::cerr << "Heading offset workflow test failed: " << error.what() << std::endl;
        return 1;
    }
    std::cout << "Heading offset workflow tests passed." << std::endl;
    return 0;
}
