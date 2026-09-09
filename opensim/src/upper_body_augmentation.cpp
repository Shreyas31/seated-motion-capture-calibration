#include "upper_body_augmentation.h"

#include <OpenSim/OpenSim.h>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>

namespace {

constexpr double kReferenceShoulderGirdleWidth = 0.16;
constexpr double kReferenceNeckOffset = 0.10;

// Small positive placeholder mass for kinematic visualisation.
// Do not use these inertial properties for dynamics.
constexpr double kAuxiliaryBodyMass = 0.001;
constexpr double kAuxiliaryInertia = 1e-5;

OpenSim::Body* createAuxiliaryBody(const std::string& name) {
    return new OpenSim::Body{
        name, kAuxiliaryBodyMass, SimTK::Vec3{0.0},
        SimTK::Inertia{kAuxiliaryInertia, kAuxiliaryInertia, kAuxiliaryInertia, 0.0, 0.0, 0.0}};
}

void configureBallJoint(OpenSim::BallJoint& joint,
                        const std::array<std::string, 3>& coordinateNames,
                        double maximumAngleRadians) {
    for (int index = 0; index < 3; ++index) {
        auto& coordinate = joint.upd_coordinates(index);

        coordinate.setName(coordinateNames.at(index));
        coordinate.setDefaultValue(0.0);

        double range[2]{-maximumAngleRadians, maximumAngleRadians};

        coordinate.setRange(range);
        coordinate.set_clamped(true);
        coordinate.set_locked(false);
    }
}

OpenSim::PhysicalOffsetFrame& getParentOffsetFrame(OpenSim::Model& model,
                                                   const std::string& jointName) {
    auto& joint = model.updJointSet().get(jointName);

    if (joint.getProperty_frames().size() < 2) {
        throw std::runtime_error("Joint '" + jointName +
                                 "' does not contain the expected offset frames.");
    }

    auto* parentOffset = dynamic_cast<OpenSim::PhysicalOffsetFrame*>(&joint.upd_frames(0));

    if (parentOffset == nullptr) {
        throw std::runtime_error("Parent frame of joint '" + jointName +
                                 "' is not a PhysicalOffsetFrame.");
    }

    return *parentOffset;
}

void attachImuFrame(OpenSim::Body& body, const std::string& frameName) {
    auto* frame = new OpenSim::PhysicalOffsetFrame{frameName, body, SimTK::Transform{}};

    body.addComponent(frame);
}

void attachShoulderGirdleGeometry(OpenSim::Body& shoulderGirdle, const std::string& frameName,
                                  const SimTK::Vec3& sternoclavicularPoint,
                                  const SimTK::Vec3& shoulderPoint, bool rightSide,
                                  double heightScale) {
    const SimTK::Vec3 midpoint = 0.5 * (sternoclavicularPoint + shoulderPoint);

    const double shoulderGirdleWidth = (shoulderPoint - sternoclavicularPoint).norm();

    const double angle = rightSide ? 0.5 * SimTK::Pi : -0.5 * SimTK::Pi;

    auto* geometryFrame = new OpenSim::PhysicalOffsetFrame{
        frameName, shoulderGirdle,
        SimTK::Transform{SimTK::Rotation{angle, SimTK::XAxis}, midpoint}};

    auto* cylinder = new OpenSim::Cylinder{0.012 * heightScale, 0.5 * shoulderGirdleWidth};

    cylinder->setName(frameName + "_proxy_cylinder");
    cylinder->setColor(SimTK::Vec3{0.85, 0.75, 0.60});

    geometryFrame->attachGeometry(cylinder);
    shoulderGirdle.addComponent(geometryFrame);
}

void moveHeadMeshesToHeadBody(OpenSim::Model& model, OpenSim::Body& head, double heightScale) {
    // Hide the head meshes that are rigidly attached to Rajagopal's torso.
    for (auto& mesh : model.updComponentList<OpenSim::Mesh>()) {
        const std::string file = mesh.get_mesh_file();
        const std::string path = mesh.getAbsolutePathString();

        const bool belongsToTorso = path.find("/bodyset/torso/") == 0;

        const bool isHeadMesh = file == "hat_skull.vtp" || file == "hat_jaw.vtp";

        if (belongsToTorso && isHeadMesh) {
            mesh.upd_Appearance().set_visible(false);
        }
    }

    // Reattach visible copies to the new articulating head body.
    auto* skull = new OpenSim::Mesh{"hat_skull.vtp"};
    skull->setName("head_skull_mesh");
    skull->set_scale_factors(SimTK::Vec3{heightScale});
    head.attachGeometry(skull);

    auto* jaw = new OpenSim::Mesh{"hat_jaw.vtp"};
    jaw->setName("head_jaw_mesh");
    jaw->set_scale_factors(SimTK::Vec3{heightScale});
    head.attachGeometry(jaw);
}

void addHead(OpenSim::Model& model, const SimTK::Vec3& rightShoulderPoint,
             const SimTK::Vec3& leftShoulderPoint, double heightScale) {
    auto& torso = model.updBodySet().get("torso");

    const SimTK::Vec3 shoulderMidpoint = 0.5 * (rightShoulderPoint + leftShoulderPoint);

    const SimTK::Vec3 neckPoint =
        shoulderMidpoint + SimTK::Vec3{0.0, kReferenceNeckOffset * heightScale, 0.0};

    auto* head = createAuxiliaryBody("head_mocap");
    model.addBody(head);

    // The head body frame coincides with the torso frame in the neutral
    // position. Using the same joint location in both frames establishes
    // that relationship while allowing rotation about the neck point.
    auto* neckJoint = new OpenSim::BallJoint{
        "neck_mocap", torso, neckPoint, SimTK::Vec3{0.0}, *head, neckPoint, SimTK::Vec3{0.0}};

    configureBallJoint(*neckJoint,
                       {"neck_lateral_bending_mocap", "neck_rotation_mocap", "neck_flexion_mocap"},
                       60.0 * SimTK::Pi / 180.0);

    model.addJoint(neckJoint);

    attachImuFrame(*head, "head_mocap_imu");
    moveHeadMeshesToHeadBody(model, *head, heightScale);
}

OpenSim::Body* addShoulderGirdleProxy(OpenSim::Model& model, const std::string& side,
                                      const SimTK::Vec3& shoulderPoint, double heightScale) {
    const bool rightSide = side == "r";
    auto& torso = model.updBodySet().get("torso");

    const double direction = rightSide ? 1.0 : -1.0;

    const double shoulderGirdleWidth = kReferenceShoulderGirdleWidth * heightScale;

    // This medial point is used only as the approximate centre of rotation
    // for the shoulder-girdle proxy. It is not a measured sternoclavicular
    // joint centre.
    SimTK::Vec3 proxyJointPoint = shoulderPoint;

    proxyJointPoint[2] -= direction * shoulderGirdleWidth;

    const std::string bodyName = "shoulder_girdle_" + side + "_mocap";

    const std::string jointName = "scapulothoracic_" + side + "_mocap";

    auto* shoulderGirdle = createAuxiliaryBody(bodyName);

    model.addBody(shoulderGirdle);

    // Preserve the joint-frame convention that was already visually tested.
    // Sensor-to-model frame alignment handles the physical posterior sensor
    // orientation separately.
    const SimTK::Vec3 jointOrientation =
        rightSide ? SimTK::Vec3{0.0, SimTK::Pi, 0.0} : SimTK::Vec3{SimTK::Pi, 0.0, 0.0};

    auto* joint =
        new OpenSim::BallJoint{jointName,       torso,           proxyJointPoint, jointOrientation,
                               *shoulderGirdle, proxyJointPoint, jointOrientation};

    configureBallJoint(*joint,
                       {"shoulder_girdle_" + side + "_elevation",
                        "shoulder_girdle_" + side + "_protraction",
                        "shoulder_girdle_" + side + "_upward_rotation"},
                       30.0 * SimTK::Pi / 180.0);

    model.addJoint(joint);

    attachImuFrame(*shoulderGirdle, bodyName + "_imu");

    attachShoulderGirdleGeometry(*shoulderGirdle, bodyName + "_visual_frame", proxyJointPoint,
                                 shoulderPoint, rightSide, heightScale);

    return shoulderGirdle;
}

} // namespace

namespace SeatedMoCap {

void augmentRajagopalUpperBody(OpenSim::Model& model, double heightScale) {
    if (!std::isfinite(heightScale) || heightScale <= 0.0) {

        throw std::invalid_argument("heightScale must be positive and finite.");
    }

    // These are the existing parent frames of Rajagopal's acromial joints.
    // Their translations have already been updated by model scaling.
    auto& rightAcromialParent = getParentOffsetFrame(model, "acromial_r");

    auto& leftAcromialParent = getParentOffsetFrame(model, "acromial_l");

    const SimTK::Vec3 rightShoulderPoint = rightAcromialParent.get_translation();

    const SimTK::Vec3 leftShoulderPoint = leftAcromialParent.get_translation();

    OpenSim::Body* rightShoulderGirdle =
        addShoulderGirdleProxy(model, "r", rightShoulderPoint, heightScale);

    OpenSim::Body* leftShoulderGirdle =
        addShoulderGirdleProxy(model, "l", leftShoulderPoint, heightScale);

    // Insert each shoulder-girdle proxy into the torso-to-humerus chain:
    //
    // torso -> shoulder-girdle proxy -> original acromial joint -> humerus
    //
    // The proxy body frame coincides with the torso frame in neutral, so the
    // original acromial offset translation remains valid.
    rightAcromialParent.setParentFrame(*rightShoulderGirdle);
    leftAcromialParent.setParentFrame(*leftShoulderGirdle);

    addHead(model, rightShoulderPoint, leftShoulderPoint, heightScale);
}

} // namespace SeatedMoCap
