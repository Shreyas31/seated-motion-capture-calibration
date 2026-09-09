#include "heading_offset_workflow.h"

#include "frontend_protocol.h"

#include <algorithm>
#include <cmath>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>

namespace xsens_heading {

namespace {

constexpr double OFFSET_TOLERANCE_DEGREES = 1e-3;

} // namespace

bool HeadingOffsetAudit::requiresReset() const noexcept {
    return !allReadable || !allZero || !allEqual;
}

HeadingOffsetService::HeadingOffsetService(std::ostream& output, std::ostream& errorOutput)
    : output_{output}, errorOutput_{errorOutput} {}

HeadingOffsetAudit HeadingOffsetService::summarize(const std::vector<double>& offsetsDegrees) {
    HeadingOffsetAudit audit;
    bool firstValidOffset = true;

    for (const double offset : offsetsDegrees) {
        if (!std::isfinite(offset)) {
            audit.allReadable = false;
            continue;
        }

        audit.allZero = audit.allZero && std::abs(offset) <= OFFSET_TOLERANCE_DEGREES;

        if (firstValidOffset) {
            audit.minimumDegrees = offset;
            audit.maximumDegrees = offset;
            firstValidOffset = false;
        } else {
            audit.minimumDegrees = (std::min)(audit.minimumDegrees, offset);
            audit.maximumDegrees = (std::max)(audit.maximumDegrees, offset);
        }
    }

    if (firstValidOffset) {
        audit.allReadable = false;
        audit.allEqual = false;
    } else {
        audit.allEqual = (audit.maximumDegrees - audit.minimumDegrees) <= OFFSET_TOLERANCE_DEGREES;
    }

    return audit;
}

HeadingOffsetAudit HeadingOffsetService::audit(const std::vector<XsDevicePtr>& devices) const {
    output_ << "\nChecking stored Xsens heading offsets..." << std::endl;
    std::vector<double> offsets;
    offsets.reserve(devices.size());

    for (const XsDevicePtr& device : devices) {
        const double offset = device->headingOffset();
        const std::string sensorId = device->deviceId().toString().toStdString();
        offsets.push_back(offset);

        if (!std::isfinite(offset)) {
            errorOutput_ << "  " << sensorId << ": heading offset could not be read." << std::endl;
            continue;
        }

        output_ << "  " << sensorId << ": " << offset << " degrees" << std::endl;
    }

    const HeadingOffsetAudit result = summarize(offsets);

    if (result.requiresReset()) {
        errorOutput_ << "WARNING: Device heading offsets are non-zero, inconsistent, "
                     << "or unreadable. They act before the software q_CG transform "
                     << "and can otherwise be mistaken for sensor mounting differences."
                     << std::endl;
    } else {
        output_ << "All device heading offsets are zero and consistent." << std::endl;
    }

    return result;
}

bool HeadingOffsetService::resetToZero(const std::vector<XsDevicePtr>& devices) const {
    bool success = true;

    for (const XsDevicePtr& device : devices) {
        const std::string sensorId = device->deviceId().toString().toStdString();

        if (!device->setHeadingOffset(0.0)) {
            errorOutput_ << "Failed to reset heading offset for " << sensorId << "." << std::endl;
            success = false;
            continue;
        }

        const double verifiedOffset = device->headingOffset();

        if (!std::isfinite(verifiedOffset) || std::abs(verifiedOffset) > OFFSET_TOLERANCE_DEGREES) {
            errorOutput_ << "Heading-offset verification failed for " << sensorId << "; read back "
                         << verifiedOffset << " degrees." << std::endl;
            success = false;
        }
    }

    return success;
}

HeadingResetDecision parseHeadingResetDecision(const char choice) noexcept {

    return choice == 'y' || choice == 'Y' ? HeadingResetDecision::Reset
                                          : HeadingResetDecision::Continue;
}

HeadingOffsetWorkflow::HeadingOffsetWorkflow(HeadingOffsetService& service,
                                             frontend_protocol::EventWriter& frontend,
                                             std::istream& input, std::ostream& output,
                                             std::ostream& errorOutput)
    : service_{service}, frontend_{frontend}, input_{input}, output_{output},
      errorOutput_{errorOutput} {}

void HeadingOffsetWorkflow::verifyAndResolve(const std::vector<XsDevicePtr>& devices) const {

    const HeadingOffsetAudit audit = service_.audit(devices);
    if (!audit.requiresReset()) {
        return;
    }

    frontend_.emitState(frontend_protocol::state::kHeading);
    output_ << "Reset every connected MTw heading offset to zero? [y/N]: ";

    char choice = 'n';
    input_ >> choice;
    if (parseHeadingResetDecision(choice) == HeadingResetDecision::Continue) {

        errorOutput_ << "Continuing without changing device settings. The raw "
                     << "q_GS data already include the offsets shown above." << std::endl;
        return;
    }

    if (!service_.resetToZero(devices)) {
        throw std::runtime_error("One or more MTw heading offsets could not be reset "
                                 "and verified. Resolve the device configuration before "
                                 "calibrating.");
    }

    output_ << "All heading offsets were reset and verified at zero." << std::endl;
}

} // namespace xsens_heading
