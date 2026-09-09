#include "frontend_protocol.h"

#include <chrono>
#include <ostream>
#include <thread>

namespace frontend_protocol {

EventWriter::EventWriter(std::ostream& output) : output_{output} {}

std::string EventWriter::formatEvent(const std::string_view category, const std::string_view name,
                                     const std::string_view detail) {

    std::string event{"FRONTEND|"};
    event += category;
    event += '|';
    event += name;

    if (!detail.empty()) {
        event += '|';
        event += detail;
    }

    return event;
}

void EventWriter::emitEvent(const std::string_view category, const std::string_view name,
                            const std::string_view detail) const {

    output_ << '\n' << formatEvent(category, name, detail) << std::endl;
}

void EventWriter::emitState(const std::string_view state) const {
    emitEvent(category::kState, state);
}

void EventWriter::emitResult(const std::string_view result, const std::string_view detail) const {

    emitEvent(category::kResult, result, detail);
}

void EventWriter::emitError(const std::string_view error, const std::string_view detail) const {

    emitEvent(category::kError, error, detail);
}

void EventWriter::emitGuidance(const std::string_view phase, const std::string_view detail) const {

    emitEvent(category::kGuidance, phase, detail);
}

void EventWriter::runGuidedCountdown(const std::string_view phase, const int seconds) const {

    for (int remaining = seconds; remaining > 0; --remaining) {

        emitGuidance(guidance::kCountdown, std::string{phase} + ":" + std::to_string(remaining));

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

} // namespace frontend_protocol
