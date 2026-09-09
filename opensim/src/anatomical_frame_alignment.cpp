#include "anatomical_frame_alignment.h"

#include "segment_model_map.h"

#include <OpenSim/OpenSim.h>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

bool isDependentKneeCoordinate(const std::string& name) {
    return name == "knee_angle_r_beta" || name == "knee_angle_l_beta";
}

OpenSim::PhysicalOffsetFrame& requireImuFrame(OpenSim::Model& model, const std::string& name) {
    OpenSim::PhysicalOffsetFrame* result = nullptr;
    int matches = 0;

    for (auto& frame : model.updComponentList<OpenSim::PhysicalOffsetFrame>()) {

        if (frame.getName() == name) {
            result = &frame;
            ++matches;
        }
    }

    if (matches == 0) {
        throw std::runtime_error("Virtual IMU frame is missing: " + name);
    }

    if (matches > 1) {
        throw std::runtime_error("Virtual IMU frame name is not unique: " + name);
    }

    return *result;
}

void setNeutralStandingState(OpenSim::Model& model, SimTK::State& state) {
    auto& coordinates = model.updCoordinateSet();

    for (int index = 0; index < coordinates.getSize(); ++index) {

        auto& coordinate = coordinates.get(index);

        const std::string& name = coordinate.getName();

        // These coordinates are driven by Rajagopal's
        // patellofemoral coupling and are not independent.
        if (isDependentKneeCoordinate(name)) {
            continue;
        }

        constexpr double neutralValue = 0.0;

        if (neutralValue < coordinate.getRangeMin() || neutralValue > coordinate.getRangeMax()) {

            throw std::runtime_error("Zero is outside the coordinate range for "
                                     "the neutral standing reference: " +
                                     name);
        }

        // Locked coordinates already have their required
        // model defaults. The current policy locks pelvis
        // translation and MTP coordinates at zero.
        if (coordinate.get_locked()) {
            const double currentValue = coordinate.getValue(state);

            if (std::abs(currentValue) > 1e-10) {
                throw std::runtime_error("Locked neutral coordinate is not zero: " + name);
            }

            continue;
        }

        coordinate.setValue(state, neutralValue, false);
    }

    // We only need body orientations. Do not run pose IK or use
    // chair/bed targets while defining these fixed frames.
    model.realizePosition(state);
}

SimTK::Rotation expectedStandingRotation(const std::string& segment) {
    // For ordinary segments in neutral standing:
    //
    // +X anatomical = forward = +X OpenSim
    // +Y anatomical = proximal/up = +Y OpenSim
    // +Z anatomical = subject right = +Z OpenSim
    //
    // Therefore R_OB is identity.
    if (segment == "Right_Shoulder") {
        // Right shoulder-girdle/scapular proxy frame:
        // +Y runs from sternum toward the right shoulder.
        return SimTK::Rotation{0.5 * SimTK::Pi, SimTK::XAxis};
    }

    if (segment == "Left_Shoulder") {
        // Left shoulder-girdle/scapular proxy frame:
        // +Y runs from sternum toward the left shoulder.
        return SimTK::Rotation{-0.5 * SimTK::Pi, SimTK::XAxis};
    }

    return SimTK::Rotation{};
}

} // namespace

namespace SeatedMoCap {

void alignAllVirtualImuFrames(OpenSim::Model& model) {
    // Initialise the augmented, scaled Rajagopal model and place
    // its independent coordinates in the neutral zero state.
    model.finalizeConnections();
    SimTK::State& state = model.initSystem();

    setNeutralStandingState(model, state);

    std::cout << "\nNeutral-standing anatomical frame alignment\n";

    for (const auto& mapping : kSegmentModelMappings) {

        const std::string segment{mapping.backendSegment};

        const std::string bodyName{mapping.opensimBody};

        const std::string frameName{mapping.imuFrame};

        const auto& body = model.getBodySet().get(bodyName);

        auto& imuFrame = requireImuFrame(model, frameName);

        // R_OM:
        // Rajagopal body frame M expressed in OpenSim
        // ground O in neutral standing.
        const SimTK::Rotation rotationOM = body.getTransformInGround(state).R();

        // R_OF:
        // Expected project anatomical frame F expressed
        // in OpenSim ground in neutral standing.
        const SimTK::Rotation rotationOF = expectedStandingRotation(segment);

        // R_OF = R_OM R_MF
        //
        // Therefore the fixed body-to-anatomical-frame
        // correction is:
        //
        // R_MF = inverse(R_OM) R_OF
        const SimTK::Rotation rotationMF = (~rotationOM) * rotationOF;

        // Preserve the frame translation. It remains zero for
        // the current orientation-only system but may be assigned
        // a physical sensor location later.
        const SimTK::Vec3 translationMF = imuFrame.getOffsetTransform().p();

        imuFrame.setOffsetTransform(SimTK::Transform{rotationMF, translationMF});

        const SimTK::Quaternion quaternionMF = rotationMF.convertRotationToQuaternion();

        const SimTK::Vec4 angleAxis = rotationMF.convertRotationToAngleAxis();

        const double correctionDegrees = std::abs(angleAxis[0]) * (180.0 / SimTK::Pi);

        std::cout << "[ALIGNED] " << segment << " | " << frameName << '\n'
                  << "  q_MF wxyz = " << quaternionMF[0] << ", " << quaternionMF[1] << ", "
                  << quaternionMF[2] << ", " << quaternionMF[3] << '\n'
                  << "  correction angle = " << correctionDegrees << " degrees\n";
    }
}

} // namespace SeatedMoCap
