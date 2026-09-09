#include "seated_pose_targets.h"

#include <stdexcept>

using namespace Eigen;

namespace {
// Build q_CB using +X_B anterior, +Y_B proximal and +Z_B subject-right.
Quaterniond makeIsbSegmentFrame(const Vector3d& proximal_axis_C, const Vector3d& anterior_hint_C) {

    const Vector3d y_B_in_C = proximal_axis_C.normalized();

    // Project the anterior hint onto the plane normal to the longitudinal axis.
    Vector3d x_B_in_C = anterior_hint_C - y_B_in_C * y_B_in_C.dot(anterior_hint_C);

    if (x_B_in_C.norm() < 1e-9) {
        throw std::invalid_argument("Anterior and proximal axes must not be parallel.");
    }

    x_B_in_C.normalize();

    const Vector3d z_B_in_C = x_B_in_C.cross(y_B_in_C).normalized();

    Matrix3d rotation;
    rotation.col(0) = x_B_in_C;
    rotation.col(1) = y_B_in_C;
    rotation.col(2) = z_B_in_C;

    return Quaterniond(rotation).normalized();
}
} // namespace

Vector3d buildForearmElbowAxisTarget(const bool rightSide) {
    // Palms-together forearm frames use -X_B on the right and +X_B on the left.
    Vector3d axis = Vector3d::UnitX();

    if (rightSide) {
        axis = -axis;
    }

    return axis;
}

std::map<std::string, Quaterniond> buildChairPoseTargets() {
    std::map<std::string, Quaterniond> targets;

    // Session axes are +X forward, +Y left and +Z up.
    const Vector3d forward = Vector3d::UnitX();
    const Vector3d left = Vector3d::UnitY();
    const Vector3d up = Vector3d::UnitZ();

    const Vector3d backward = -forward;
    const Vector3d right = -left;

    targets["Head"] = makeIsbSegmentFrame(up, forward);

    targets["Sternum"] = makeIsbSegmentFrame(up, forward);

    targets["Pelvis"] = makeIsbSegmentFrame(up, forward);

    // Seated thighs have proximal axes backward and anterior surfaces upward.
    targets["Right_Upperleg"] = makeIsbSegmentFrame(backward, up);

    targets["Left_Upperleg"] = makeIsbSegmentFrame(backward, up);

    targets["Right_Lowerleg"] = makeIsbSegmentFrame(up, forward);

    targets["Left_Lowerleg"] = makeIsbSegmentFrame(up, forward);

    targets["Right_Foot"] = makeIsbSegmentFrame(up, forward);

    targets["Left_Foot"] = makeIsbSegmentFrame(up, forward);

    targets["Right_Upperarm"] = makeIsbSegmentFrame(up, forward);

    targets["Left_Upperarm"] = makeIsbSegmentFrame(up, forward);

    // Forward forearms have proximal axes backward; palm normals face inward.
    const Quaterniond rightForearmNeutral = makeIsbSegmentFrame(backward, left);

    const Quaterniond leftForearmNeutral = makeIsbSegmentFrame(backward, right);

    targets["Right_Forearm"] = rightForearmNeutral;
    targets["Left_Forearm"] = leftForearmNeutral;

    targets["Right_Hand"] = rightForearmNeutral;
    targets["Left_Hand"] = leftForearmNeutral;

    // Approximate shoulder frames use the torso-to-shoulder direction as +Y_B.
    targets["Right_Shoulder"] = makeIsbSegmentFrame(right, forward);

    targets["Left_Shoulder"] = makeIsbSegmentFrame(left, forward);

    return targets;
}

std::map<std::string, Quaterniond> buildBedPoseTargets() {
    std::map<std::string, Quaterniond> targets;

    // Session axes are +X toward the feet, +Y left and +Z up.
    const Vector3d forward = Vector3d::UnitX();
    const Vector3d left = Vector3d::UnitY();
    const Vector3d up = Vector3d::UnitZ();

    const Vector3d backward = -forward;
    const Vector3d right = -left;

    targets["Head"] = makeIsbSegmentFrame(up, forward);

    targets["Sternum"] = makeIsbSegmentFrame(up, forward);

    targets["Pelvis"] = makeIsbSegmentFrame(up, forward);

    // Extended legs have proximal axes backward and anterior surfaces upward.
    targets["Right_Upperleg"] = makeIsbSegmentFrame(backward, up);

    targets["Left_Upperleg"] = makeIsbSegmentFrame(backward, up);

    targets["Right_Lowerleg"] = makeIsbSegmentFrame(backward, up);

    targets["Left_Lowerleg"] = makeIsbSegmentFrame(backward, up);

    // Upward toe axes keep the bed-pose ankles approximately neutral.
    targets["Right_Foot"] = makeIsbSegmentFrame(backward, up);

    targets["Left_Foot"] = makeIsbSegmentFrame(backward, up);

    targets["Right_Upperarm"] = makeIsbSegmentFrame(up, forward);

    targets["Left_Upperarm"] = makeIsbSegmentFrame(up, forward);

    // Forward forearms have proximal axes backward; palm normals face inward.
    const Quaterniond rightForearmNeutral = makeIsbSegmentFrame(backward, left);

    const Quaterniond leftForearmNeutral = makeIsbSegmentFrame(backward, right);

    targets["Right_Forearm"] = rightForearmNeutral;
    targets["Left_Forearm"] = leftForearmNeutral;

    targets["Right_Hand"] = rightForearmNeutral;
    targets["Left_Hand"] = leftForearmNeutral;

    // Approximate shoulder frames use the torso-to-shoulder direction as +Y_B.
    targets["Right_Shoulder"] = makeIsbSegmentFrame(right, forward);

    targets["Left_Shoulder"] = makeIsbSegmentFrame(left, forward);

    return targets;
}
