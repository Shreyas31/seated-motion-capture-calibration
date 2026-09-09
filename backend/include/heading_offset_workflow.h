#pragma once

#include <iosfwd>
#include <vector>
#include <xsensdeviceapi.h>

namespace frontend_protocol {
class EventWriter;
}

namespace xsens_heading {

/**
 * Summarises readability and consistency of stored device heading offsets.
 * @struct HeadingOffsetAudit.
 */
struct HeadingOffsetAudit {
    bool allReadable = true;
    bool allZero = true;
    bool allEqual = true;
    double minimumDegrees = 0.0;
    double maximumDegrees = 0.0;

    /**
     * Reports whether unreadable, non-zero, or inconsistent offsets require attention.
     * @return True when the audit is not entirely readable, zero, and equal.
     */
    bool requiresReset() const noexcept;
};

/**
 * Reads, summarises, resets, and verifies Xsens device heading offsets.
 * @class HeadingOffsetService.
 */
class HeadingOffsetService {
  public:
    /**
     * Creates a heading-offset service using injected report streams.
     * @param output Stream used for normal audit and reset results.
     * @param errorOutput Stream used for unreadable offsets and reset failures.
     */
    HeadingOffsetService(std::ostream& output, std::ostream& errorOutput);

    /**
     * Reads and reports stored heading offsets for connected devices.
     * @param devices Connected MTw devices to inspect.
     * @return Audit containing readability, equality, and degree range.
     */
    HeadingOffsetAudit audit(const std::vector<XsDevicePtr>& devices) const;

    /**
     * Sets every device heading offset to zero and verifies each readback.
     * @param devices Connected MTw devices to modify.
     * @return True only when every reset succeeds and verifies within tolerance.
     */
    bool resetToZero(const std::vector<XsDevicePtr>& devices) const;

    /**
     * Computes audit flags and range from heading-offset values.
     * @param offsetsDegrees Heading offsets in degrees; non-finite values are unreadable.
     * @return Aggregate audit for the supplied values.
     */
    static HeadingOffsetAudit summarize(const std::vector<double>& offsetsDegrees);

  private:
    std::ostream& output_;
    std::ostream& errorOutput_;
};

/**
 * Identifies whether stored hardware heading offsets should be reset.
 * @enum HeadingResetDecision.
 */
enum class HeadingResetDecision { Reset, Continue };

/**
 * Converts a confirmation character into a heading-reset decision.
 * @param choice Clinician response character.
 * @return Reset for y or Y; otherwise Continue.
 */
HeadingResetDecision parseHeadingResetDecision(char choice) noexcept;

/**
 * Audits heading offsets and coordinates an optional clinician-approved reset.
 * @class HeadingOffsetWorkflow.
 */
class HeadingOffsetWorkflow {
  public:
    /**
     * Creates a heading-resolution workflow using injected services and streams.
     * @param service Service that reads, resets, and verifies device offsets.
     * @param frontend Writer for structured heading-state events.
     * @param input Stream used to read reset confirmation.
     * @param output Stream used for prompts and successful reset messages.
     * @param errorOutput Stream used when continuing with unresolved offsets.
     */
    HeadingOffsetWorkflow(HeadingOffsetService& service, frontend_protocol::EventWriter& frontend,
                          std::istream& input, std::ostream& output, std::ostream& errorOutput);

    /**
     * Audits offsets and, when requested, resets every connected device before measurement mode.
     * @param devices Connected MTw devices still in configuration mode.
     * @throws std::runtime_error if an approved reset cannot be completed and verified.
     */
    void verifyAndResolve(const std::vector<XsDevicePtr>& devices) const;

  private:
    HeadingOffsetService& service_;
    frontend_protocol::EventWriter& frontend_;
    std::istream& input_;
    std::ostream& output_;
    std::ostream& errorOutput_;
};

} // namespace xsens_heading
