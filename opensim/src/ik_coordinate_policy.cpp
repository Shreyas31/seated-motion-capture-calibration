#include "ik_coordinate_policy.h"

#include <OpenSim/OpenSim.h>
#include <stdexcept>
#include <string>

namespace {

OpenSim::Coordinate& requireCoordinate(OpenSim::Model& model, const std::string& name) {
    auto& coordinates = model.updCoordinateSet();

    for (int index = 0; index < coordinates.getSize(); ++index) {

        auto& coordinate = coordinates.get(index);

        if (coordinate.getName() == name) {
            return coordinate;
        }
    }

    throw std::runtime_error("Required IK coordinate is missing: " + name);
}

void configureUnlocked(OpenSim::Model& model, const std::string& name) {
    auto& coordinate = requireCoordinate(model, name);

    coordinate.set_locked(false);
    coordinate.set_clamped(true);
}

void configurePalmRotation(OpenSim::Model& model, const std::string& name) {
    auto& coordinate = requireCoordinate(model, name);

    // Calibration conventions:
    //   0      = palm upward
    //   pi / 2 = palms facing each other
    //   pi     = palm downward
    coordinate.setRangeMin(0.0);
    coordinate.setRangeMax(SimTK::Pi);
    coordinate.set_locked(false);
    coordinate.set_clamped(true);
}

void configureArmFlexion(OpenSim::Model& model, const std::string& name) {
    auto& coordinate = requireCoordinate(model, name);

    // The original Rajagopal coordinate is limited to approximately
    // +/-90 degrees. Orientation IK therefore cannot represent an arm
    // moving through the overhead position and may compensate with
    // shoulder rotation or elbow flexion instead.
    //
    // Extend the permitted interval symmetrically to +/-180 degrees.
    // Clamping remains enabled so IK cannot leave this explicit range.
    coordinate.setRangeMin(-SimTK::Pi);
    coordinate.setRangeMax(SimTK::Pi);
    coordinate.set_locked(false);
    coordinate.set_clamped(true);
}

void configureLocked(OpenSim::Model& model, const std::string& name, double defaultValue) {
    auto& coordinate = requireCoordinate(model, name);

    coordinate.setDefaultValue(defaultValue);
    coordinate.set_locked(true);
}

} // namespace

namespace SeatedMoCap {

void configureCoordinatesForOrientationIk(OpenSim::Model& model) {
    // Global translation cannot be determined from orientations.
    for (const char* name : {"pelvis_tx", "pelvis_ty", "pelvis_tz"}) {

        configureLocked(model, name, 0.0);
    }

    // Pelvis orientation is constrained by the pelvis IMU.
    for (const char* name : {"pelvis_tilt", "pelvis_list", "pelvis_rotation"}) {

        configureUnlocked(model, name);
    }

    // Lower limbs.
    for (const char* name : {"hip_flexion_r", "hip_adduction_r", "hip_rotation_r", "knee_angle_r",
                             "ankle_angle_r", "subtalar_angle_r",

                             "hip_flexion_l", "hip_adduction_l", "hip_rotation_l", "knee_angle_l",
                             "ankle_angle_l", "subtalar_angle_l"}) {

        configureUnlocked(model, name);
    }

    // No toe IMUs are present.
    configureLocked(model, "mtp_angle_r", 0.0);
    configureLocked(model, "mtp_angle_l", 0.0);

    // Trunk orientation is constrained by pelvis and torso IMUs.
    for (const char* name : {"lumbar_extension", "lumbar_bending", "lumbar_rotation"}) {

        configureUnlocked(model, name);
    }

    // Right upper limb, excluding arm flexion configured below.
    for (const char* name :
         {"arm_add_r", "arm_rot_r", "elbow_flex_r", "wrist_flex_r", "wrist_dev_r"}) {

        configureUnlocked(model, name);
    }

    // Left upper limb, excluding arm flexion configured below.
    for (const char* name :
         {"arm_add_l", "arm_rot_l", "elbow_flex_l", "wrist_flex_l", "wrist_dev_l"}) {

        configureUnlocked(model, name);
    }

    configureArmFlexion(model, "arm_flex_r");
    configureArmFlexion(model, "arm_flex_l");

    // The original Rajagopal model stops at pi/2. Extend the
    // coordinate to pi so calibration can represent palms down.
    configurePalmRotation(model, "pro_sup_r");
    configurePalmRotation(model, "pro_sup_l");

    // Added shoulder-girdle proxy coordinates.
    for (const char* name : {"shoulder_girdle_r_elevation", "shoulder_girdle_r_protraction",
                             "shoulder_girdle_r_upward_rotation",

                             "shoulder_girdle_l_elevation", "shoulder_girdle_l_protraction",
                             "shoulder_girdle_l_upward_rotation"}) {

        configureUnlocked(model, name);
    }

    // Added neck coordinates.
    for (const char* name :
         {"neck_lateral_bending_mocap", "neck_rotation_mocap", "neck_flexion_mocap"}) {

        configureUnlocked(model, name);
    }

    // Do not modify:
    //
    // knee_angle_r_beta
    // knee_angle_l_beta
    //
    // They belong to Rajagopal's coupled knee/patellofemoral mechanics
    // and are not independent IMU-driven coordinates.
}

} // namespace SeatedMoCap
