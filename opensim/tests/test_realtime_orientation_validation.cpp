#include "orientation_stream.h"

#include <cassert>
#include <cstring>
#include <limits>

namespace {

SeatedMoCap::Realtime::OrientationFrameV1 makeValidFrame() {
    SeatedMoCap::Realtime::OrientationFrameV1 frame{};
    std::memcpy(frame.magic, "SMC1", 4);
    frame.version = SeatedMoCap::Realtime::kProtocolVersion;
    frame.sensorCount = SeatedMoCap::Realtime::kSensorCount;
    frame.validSensorMask = SeatedMoCap::Realtime::kAllSensorsValidMask;

    for (auto& orientation : frame.orientations) {
        orientation.w = 1.0F;
    }
    return frame;
}

void expectInvalid(const SeatedMoCap::Realtime::OrientationFrameV1& frame) {
    const auto validation = SeatedMoCap::Realtime::validateOrientationFrame(frame);
    assert(!validation.valid);
    assert(!validation.errorMessage.empty());
}

} // namespace

int main() {
    auto frame = makeValidFrame();
    const auto validResult = SeatedMoCap::Realtime::validateOrientationFrame(frame);
    assert(validResult.valid);
    assert(validResult.errorMessage.empty());

    frame = makeValidFrame();
    frame.magic[0] = 'X';
    expectInvalid(frame);

    frame = makeValidFrame();
    ++frame.version;
    expectInvalid(frame);

    frame = makeValidFrame();
    --frame.sensorCount;
    expectInvalid(frame);

    frame = makeValidFrame();
    frame.validSensorMask = 0;
    expectInvalid(frame);

    frame = makeValidFrame();
    frame.orientations[3].w = 0.5F;
    expectInvalid(frame);

    frame = makeValidFrame();
    frame.orientations[3].w = std::numeric_limits<float>::quiet_NaN();
    expectInvalid(frame);
    return 0;
}
