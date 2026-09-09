#include "heading_offset_workflow.h"

#include <cmath>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

void expect(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testZeroConsistentOffsetsNeedNoReset() {
    const auto audit = xsens_heading::HeadingOffsetService::summarize({0.0, 0.0005, -0.0004});

    expect(audit.allReadable, "Zero offsets were not readable.");
    expect(audit.allZero, "Tolerance-compatible offsets were not zero.");
    expect(audit.allEqual, "Tolerance-compatible offsets were unequal.");
    expect(!audit.requiresReset(), "Valid offsets requested a reset.");
}

void testEqualNonzeroOffsetsNeedReset() {
    const auto audit = xsens_heading::HeadingOffsetService::summarize({4.0, 4.0005});

    expect(audit.allReadable, "Finite offsets were unreadable.");
    expect(!audit.allZero, "Nonzero offsets were classified as zero.");
    expect(audit.allEqual, "Equal nonzero offsets were inconsistent.");
    expect(audit.requiresReset(), "Nonzero offsets did not request reset.");
}

void testInconsistentOrUnreadableOffsetsNeedReset() {
    const auto inconsistent = xsens_heading::HeadingOffsetService::summarize({0.0, 2.0});
    const auto unreadable = xsens_heading::HeadingOffsetService::summarize(
        {0.0, std::numeric_limits<double>::quiet_NaN()});

    expect(!inconsistent.allEqual, "Different offsets were equal.");
    expect(inconsistent.requiresReset(), "Inconsistent offsets passed.");
    expect(!unreadable.allReadable, "NaN offset was readable.");
    expect(unreadable.requiresReset(), "Unreadable offset passed.");
}

void testEmptyDeviceSetIsInvalid() {
    const auto audit = xsens_heading::HeadingOffsetService::summarize({});

    expect(!audit.allReadable, "Empty offsets were readable.");
    expect(!audit.allEqual, "Empty offsets were equal.");
    expect(audit.requiresReset(), "Empty offsets passed audit.");
}

} // namespace

int main() {
    try {
        testZeroConsistentOffsetsNeedNoReset();
        testEqualNonzeroOffsetsNeedReset();
        testInconsistentOrUnreadableOffsetsNeedReset();
        testEmptyDeviceSetIsInvalid();
    } catch (const std::exception& error) {
        std::cerr << "Heading offset service test failed: " << error.what() << std::endl;
        return 1;
    }

    std::cout << "Heading offset service tests passed." << std::endl;
    return 0;
}
