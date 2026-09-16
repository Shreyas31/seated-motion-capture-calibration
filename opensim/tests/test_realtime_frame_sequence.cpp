#include "orientation_stream.h"

#include <stdexcept>
#include <string>

namespace {

void expect(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

} // namespace

int main() {
    SeatedMoCap::Realtime::FrameSequenceTracker tracker;
    expect(!tracker.hasAcceptedFrame(), "A new tracker already contained an accepted frame.");

    auto update = tracker.observe(100, 0);
    expect(update.accepted, "The first frame was rejected.");
    expect(update.unaccountedGap == 0, "The first frame reported an unexpected gap.");
    expect(tracker.previousSequence() == 100, "The first frame sequence was not retained.");

    update = tracker.observe(101, 0);
    expect(update.accepted, "A consecutive frame was rejected.");
    expect(update.unaccountedGap == 0, "A consecutive frame reported an unexpected gap.");

    update = tracker.observe(106, 2);
    expect(update.accepted, "A newer frame with a reported superseded count was rejected.");
    expect(update.unaccountedGap == 2, "The unexplained frame-gap count was incorrect.");
    expect(tracker.previousSequence() == 106, "The latest accepted sequence was not retained.");

    update = tracker.observe(106, 0);
    expect(!update.accepted, "A duplicate frame was accepted.");
    expect(tracker.previousSequence() == 106, "A duplicate frame changed the retained sequence.");

    update = tracker.observe(105, 0);
    expect(!update.accepted, "An out-of-order frame was accepted.");
    expect(tracker.previousSequence() == 106,
           "An out-of-order frame changed the retained sequence.");

    update = tracker.observe(110, 10);
    expect(update.accepted, "A newer frame was rejected.");
    expect(update.unaccountedGap == 0,
           "Superseded frames were incorrectly reported as an unexplained gap.");
    return 0;
}
