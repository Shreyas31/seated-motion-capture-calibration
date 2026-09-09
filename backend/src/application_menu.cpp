#include "application_menu.h"

#include "frontend_protocol.h"

#include <istream>
#include <limits>
#include <ostream>

std::optional<ApplicationAction> applicationActionFromChoice(const int choice) noexcept {

    switch (choice) {
    case 1:
        return ApplicationAction::StaticChair;
    case 2:
        return ApplicationAction::StaticBed;
    case 3:
        return ApplicationAction::FunctionalCalibration;
    case 4:
        return ApplicationAction::RecordMeasurement;
    case 5:
        return ApplicationAction::Exit;
    default:
        return std::nullopt;
    }
}

std::optional<int> staticPoseChoice(const ApplicationAction action) noexcept {

    switch (action) {
    case ApplicationAction::StaticChair:
        return 1;
    case ApplicationAction::StaticBed:
        return 2;
    default:
        return std::nullopt;
    }
}

ApplicationMenu::ApplicationMenu(frontend_protocol::EventWriter& frontend, std::istream& input,
                                 std::ostream& output)
    : frontend_{frontend}, input_{input}, output_{output} {}

std::optional<ApplicationAction> ApplicationMenu::collect() const {
    frontend_.emitState(frontend_protocol::state::kMenu);
    output_ << "\nSelect Action: "
            << "\n1. Perform Static Chair Pose Calibration"
            << "\n2. Perform Static Bed Pose Calibration"
            << "\n3. Perform Functional Calibration"
            << "\n4. Start Data Recording"
            << "\n5. Exit\nChoice: ";

    int choice = 0;
    if (!(input_ >> choice)) {
        if (input_.eof()) {
            return ApplicationAction::Exit;
        }
        input_.clear();
        input_.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
        output_ << "Invalid choice. Please try again." << std::endl;
        return std::nullopt;
    }

    const auto action = applicationActionFromChoice(choice);
    if (!action) {
        output_ << "Invalid choice. Please try again." << std::endl;
    }
    return action;
}
