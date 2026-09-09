#include "segment_model_map.h"

#include <iostream>
#include <set>
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
    using namespace SeatedMoCap;

    std::set<std::string> backendSegments;
    std::set<std::string> imuFrames;

    std::size_t existingBodies = 0;
    std::size_t bodiesToAdd = 0;
    std::size_t addedBodies = 0;

    for (std::size_t index = 0; index < kSegmentModelMappings.size(); ++index) {

        const auto& mapping = kSegmentModelMappings[index];

        // The stored index must match the array/wire-protocol index.
        expect(mapping.index == index, "Stored mapping index does not match array position.");

        expect(!mapping.backendSegment.empty(), "Backend segment is empty.");
        expect(!mapping.opensimBody.empty(), "OpenSim body is empty.");
        expect(!mapping.imuFrame.empty(), "IMU frame is empty.");

        const std::string segment{mapping.backendSegment};
        const std::string body{mapping.opensimBody};
        const std::string frame{mapping.imuFrame};

        // All segment and frame names must be unique.
        expect(backendSegments.insert(segment).second, "Backend segment names are not unique.");
        expect(imuFrames.insert(frame).second, "IMU frame names are not unique.");

        // Enforce the OpenSense <body-name>_imu convention.
        expect(frame == body + "_imu", "IMU frame does not follow the <body>_imu convention.");

        if (mapping.treatment == BodyTreatment::ExistingRajagopalBody) {

            ++existingBodies;
        } else if (mapping.treatment == BodyTreatment::AddedByAugmentation) {

            ++addedBodies;
        } else {
            ++bodiesToAdd;
        }
    }

    expect(existingBodies == 14, "Unexpected existing-body count.");
    expect(bodiesToAdd == 0, "Unexpected missing-body count.");
    expect(addedBodies == 3, "Unexpected augmented-body count.");

    std::cout << "Segment-model mapping tests passed.\n"
              << "Existing Rajagopal bodies: " << existingBodies << '\n'
              << "Bodies requiring augmentation: " << bodiesToAdd << '\n'
              << "Bodies added by augmentation: " << addedBodies << '\n';

    return 0;
}
