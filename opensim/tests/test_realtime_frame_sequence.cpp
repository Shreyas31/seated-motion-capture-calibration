#include "orientation_stream.h"

#include <cassert>

int main() {
    SeatedMoCap::Realtime::FrameSequenceTracker tracker;
    assert(!tracker.hasAcceptedFrame());

    auto update = tracker.observe(100, 0);
    assert(update.accepted);
    assert(update.unaccountedGap == 0);
    assert(tracker.previousSequence() == 100);

    update = tracker.observe(101, 0);
    assert(update.accepted);
    assert(update.unaccountedGap == 0);

    update = tracker.observe(106, 2);
    assert(update.accepted);
    assert(update.unaccountedGap == 2);
    assert(tracker.previousSequence() == 106);

    update = tracker.observe(106, 0);
    assert(!update.accepted);
    assert(tracker.previousSequence() == 106);

    update = tracker.observe(105, 0);
    assert(!update.accepted);
    assert(tracker.previousSequence() == 106);

    update = tracker.observe(110, 10);
    assert(update.accepted);
    assert(update.unaccountedGap == 0);
    return 0;
}
