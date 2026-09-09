#pragma once

#include <iosfwd>
#include <optional>

namespace frontend_protocol {
class EventWriter;
}

/**
 * Identifies an action available from the clinician's main workflow menu.
 * @enum ApplicationAction.
 */
enum class ApplicationAction {
    StaticChair,
    StaticBed,
    FunctionalCalibration,
    RecordMeasurement,
    Exit
};

/**
 * Converts a numeric menu choice into an application action.
 * @param choice Integer entered by the controlling user interface.
 * @return Corresponding action, or no value when the choice is outside the menu range.
 */
std::optional<ApplicationAction> applicationActionFromChoice(int choice) noexcept;

/**
 * Converts a static-calibration action into its pose-selection number.
 * @param action Application action to inspect.
 * @return Pose number 1 or 2, or no value for non-static actions.
 */
std::optional<int> staticPoseChoice(ApplicationAction action) noexcept;

/**
 * Presents and validates one iteration of the clinician action menu.
 * @class ApplicationMenu.
 */
class ApplicationMenu {
  public:
    /**
     * Creates a menu using injected protocol and console streams.
     * @param frontend Writer for structured frontend state events.
     * @param input Stream from which the numeric choice is read.
     * @param output Stream to which the menu and validation messages are written.
     */
    ApplicationMenu(frontend_protocol::EventWriter& frontend, std::istream& input,
                    std::ostream& output);

    /**
     * Reads one menu choice without internally retrying invalid input.
     * @return Selected action, Exit on end-of-input, or no value for malformed input.
     */
    std::optional<ApplicationAction> collect() const;

  private:
    frontend_protocol::EventWriter& frontend_;
    std::istream& input_;
    std::ostream& output_;
};
