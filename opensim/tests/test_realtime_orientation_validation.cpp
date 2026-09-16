#include "orientation_stream.h"

#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

void expect(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

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
    expect(!validation.valid, "An invalid orientation frame was accepted.");
    expect(!validation.errorMessage.empty(),
           "An invalid orientation frame did not provide a rejection reason.");
}

} // namespace

int main() {
    auto frame = makeValidFrame();
    const auto validResult = SeatedMoCap::Realtime::validateOrientationFrame(frame);
    expect(validResult.valid, "A valid orientation frame was rejected.");
    expect(validResult.errorMessage.empty(),
           "A valid orientation frame produced an error message.");

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
