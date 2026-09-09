#include "coordinate_conventions.h"
#include "pose_preset.h"
#include "seated_pose_targets.h"
#include "segment_model_map.h"

#include <Eigen>
#include <OpenSim/OpenSim.h>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using Json = nlohmann::json;

using PoseTargets = std::map<std::string, Eigen::Quaterniond>;

struct PoseDefinition {
    std::string name;
    double pelvisTilt;
    double hipFlexion;
    double kneeFlexion;
    double elbowFlexion;
    double rightProSup;
    double leftProSup;
    PoseTargets idealTargets;
};

OpenSim::Coordinate& requireCoordinate(OpenSim::Model& model, const std::string& name) {
    auto& coordinates = model.updCoordinateSet();

    for (int index = 0; index < coordinates.getSize(); ++index) {

        auto& coordinate = coordinates.get(index);

        if (coordinate.getName() == name) {
            return coordinate;
        }
    }

    throw std::runtime_error("Required pose coordinate is missing: " + name);
}

const OpenSim::PhysicalOffsetFrame& requireImuFrame(const OpenSim::Model& model,
                                                    const std::string& name) {
    const OpenSim::PhysicalOffsetFrame* result = nullptr;
    int matches = 0;

    for (const auto& frame : model.getComponentList<OpenSim::PhysicalOffsetFrame>()) {

        if (frame.getName() == name) {
            result = &frame;
            ++matches;
        }
    }

    if (matches == 0) {
        throw std::runtime_error("Required virtual IMU frame is missing: " + name);
    }

    if (matches > 1) {
        throw std::runtime_error("Virtual IMU frame name is not unique: " + name);
    }

    return *result;
}

bool isDependentKneeCoordinate(const std::string& name) {
    return name == "knee_angle_r_beta" || name == "knee_angle_l_beta";
}

void setCoordinate(OpenSim::Model& model, SimTK::State& state, const std::string& name,
                   double value) {
    auto& coordinate = requireCoordinate(model, name);

    if (value < coordinate.getRangeMin() || value > coordinate.getRangeMax()) {

        throw std::runtime_error("Pose value is outside the range for " + name);
    }

    if (coordinate.getLocked(state)) {
        const double currentValue = coordinate.getValue(state);

        if (std::abs(currentValue - value) > 1e-10) {
            throw std::runtime_error("Cannot change locked coordinate '" + name + "' from " +
                                     std::to_string(currentValue) + " to " + std::to_string(value));
        }

        return;
    }

    coordinate.setValue(state, value, false);
}

void setZero(OpenSim::Model& model, SimTK::State& state, const std::string& name) {
    setCoordinate(model, state, name, 0.0);
}

void setPoseCoordinates(OpenSim::Model& model, SimTK::State& state, const PoseDefinition& pose) {
    // Absolute position remains unmeasured.
    for (const char* name : {"pelvis_tx", "pelvis_ty", "pelvis_tz"}) {

        setZero(model, state, name);
    }

    // Rajagopal pelvis_tilt rotates about OpenSim ground +Z.
    // +90 degrees rotates standing superior +Y toward -X,
    // placing the head toward -X and feet toward +X.
    setCoordinate(model, state, "pelvis_tilt", pose.pelvisTilt);

    setZero(model, state, "pelvis_list");

    setZero(model, state, "pelvis_rotation");

    // Upright torso.
    for (const char* name : {"lumbar_extension", "lumbar_bending", "lumbar_rotation"}) {

        setZero(model, state, name);
    }

    // Neutral head relative to torso.
    for (const char* name :
         {"neck_lateral_bending_mocap", "neck_rotation_mocap", "neck_flexion_mocap"}) {

        setZero(model, state, name);
    }

    for (const char* side : {"r", "l"}) {
        const std::string suffix{side};

        // Lower body.
        setCoordinate(model, state, "hip_flexion_" + suffix, pose.hipFlexion);

        setZero(model, state, "hip_adduction_" + suffix);

        setZero(model, state, "hip_rotation_" + suffix);

        setCoordinate(model, state, "knee_angle_" + suffix, pose.kneeFlexion);

        setZero(model, state, "ankle_angle_" + suffix);

        setZero(model, state, "subtalar_angle_" + suffix);

        setZero(model, state, "mtp_angle_" + suffix);

        // Neutral shoulder-girdle proxy relative to the torso.
        setZero(model, state, "shoulder_girdle_" + suffix + "_elevation");

        setZero(model, state, "shoulder_girdle_" + suffix + "_protraction");

        setZero(model, state, "shoulder_girdle_" + suffix + "_upward_rotation");

        // Upper arms hang beside the body.
        setZero(model, state, "arm_flex_" + suffix);

        setZero(model, state, "arm_add_" + suffix);

        setZero(model, state, "arm_rot_" + suffix);

        // Forearms extend forward.
        setCoordinate(model, state, "elbow_flex_" + suffix, pose.elbowFlexion);

        const double proSup = suffix == "r" ? pose.rightProSup : pose.leftProSup;

        setCoordinate(model, state, "pro_sup_" + suffix, proSup);

        // Neutral wrist.
        setZero(model, state, "wrist_flex_" + suffix);

        setZero(model, state, "wrist_dev_" + suffix);
    }
}

void enforceDependentConstraints(OpenSim::Model& model, SimTK::State& state) {
    // Temporarily lock every independent coordinate at its
    // prescribed value. Model::assemble() can then update dependent
    // coordinates such as knee_angle_*_beta without changing the
    // clinically prescribed joint angles.
    std::vector<OpenSim::Coordinate*> temporarilyLocked;

    auto& coordinates = model.updCoordinateSet();

    for (int index = 0; index < coordinates.getSize(); ++index) {

        auto& coordinate = coordinates.get(index);

        if (isDependentKneeCoordinate(coordinate.getName())) {
            continue;
        }

        if (!coordinate.getLocked(state)) {
            coordinate.setLocked(state, true);

            temporarilyLocked.push_back(&coordinate);
        }
    }

    model.assemble(state);

    for (OpenSim::Coordinate* coordinate : temporarilyLocked) {

        coordinate->setLocked(state, false);
    }

    model.realizePosition(state);
}

SimTK::Rotation idealRotationOB(const PoseTargets& targets, const std::string& segment) {
    const auto iterator = targets.find(segment);

    if (iterator == targets.end()) {
        throw std::runtime_error("Ideal pose target is missing segment: " + segment);
    }

    const Eigen::Quaterniond& quaternionCB = iterator->second;

    const SeatedMoCap::Coordinates::QuaternionWxyz input{quaternionCB.w(), quaternionCB.x(),
                                                         quaternionCB.y(), quaternionCB.z()};

    const auto quaternionOB = SeatedMoCap::Coordinates::sessionBodyToOpenSim(input);

    return SimTK::Rotation{
        SimTK::Quaternion{quaternionOB.w, quaternionOB.x, quaternionOB.y, quaternionOB.z}};
}

double orientationErrorDegrees(const SimTK::Rotation& expected, const SimTK::Rotation& actual) {
    const SimTK::Rotation difference = (~expected) * actual;

    const SimTK::Vec4 angleAxis = difference.convertRotationToAngleAxis();

    return std::abs(angleAxis[0]) * (180.0 / SimTK::Pi);
}

Json quaternionJson(const SimTK::Rotation& rotation) {
    const SimTK::Quaternion quaternion = rotation.convertRotationToQuaternion();

    return Json::array({quaternion[0], quaternion[1], quaternion[2], quaternion[3]});
}

Json generatePose(const std::filesystem::path& modelPath, const PoseDefinition& pose) {
    OpenSim::Model model{modelPath.string()};

    model.finalizeConnections();

    SimTK::State& state = model.initSystem();

    state.setTime(0.0);

    setPoseCoordinates(model, state, pose);

    enforceDependentConstraints(model, state);

    Json result;

    result["generation_method"] = "prescribed_coordinates_forward_kinematics";

    result["coordinates_rad"] = Json::object();

    result["coordinates_m"] = Json::object();

    result["model_reference_orientations_wxyz"] = Json::object();

    result["ideal_orientations_wxyz"] = Json::object();

    result["model_representation_error_deg"] = Json::object();

    // Save the realised coordinate state. This includes dependent
    // coordinates so the complete model state remains auditable.
    for (const auto& coordinate : model.getComponentList<OpenSim::Coordinate>()) {

        const std::string path = coordinate.getAbsolutePathString();

        const double value = coordinate.getValue(state);

        if (!std::isfinite(value)) {
            throw std::runtime_error("Non-finite coordinate in pose: " + path);
        }

        if (coordinate.getMotionType() == OpenSim::Coordinate::MotionType::Translational) {

            result["coordinates_m"][path] = value;
        } else {
            result["coordinates_rad"][path] = value;
        }
    }

    double squaredErrorSum = 0.0;
    double maximumError = -1.0;
    std::string worstFrame;

    for (const auto& mapping : SeatedMoCap::kSegmentModelMappings) {

        const std::string segment{mapping.backendSegment};

        const std::string frameName{mapping.imuFrame};

        const auto& frame = requireImuFrame(model, frameName);

        // This is the orientation the generated Rajagopal model
        // can actually achieve at the prescribed coordinates.
        //
        // It will later be used as the sensor-model
        // reference orientation.
        const SimTK::Rotation modelRotationOF = frame.getTransformInGround(state).R();

        // This is the independent ideal anatomical target from
        // the existing backend pose definitions.
        const SimTK::Rotation idealRotation = idealRotationOB(pose.idealTargets, segment);

        const double representationError = orientationErrorDegrees(idealRotation, modelRotationOF);

        result["model_reference_orientations_wxyz"][frameName] = quaternionJson(modelRotationOF);

        result["ideal_orientations_wxyz"][frameName] = quaternionJson(idealRotation);

        result["model_representation_error_deg"][frameName] = representationError;

        squaredErrorSum += representationError * representationError;

        if (representationError > maximumError) {

            maximumError = representationError;

            worstFrame = frameName;
        }
    }

    const double rmsError =
        std::sqrt(squaredErrorSum / static_cast<double>(SeatedMoCap::kSegmentModelMappings.size()));

    result["model_representation_summary"] = {{"rms_error_deg", rmsError},
                                              {"maximum_error_deg", maximumError},
                                              {"worst_frame", worstFrame}};

    std::cout << "\nPose: " << pose.name << '\n'
              << "  Generation: prescribed coordinates "
                 "with forward kinematics\n"
              << "  Model-representation RMS: " << rmsError << " degrees\n"
              << "  Maximum representation error: " << maximumError << " degrees\n"
              << "  Worst frame: " << worstFrame << '\n';

    return result;
}

void verifyWrittenPosePreset(const std::filesystem::path& modelPath,
                             const std::filesystem::path& presetPath, const std::string& poseName) {
    const auto preset = SeatedMoCap::loadPosePreset(presetPath, poseName);

    if (modelPath.filename().string() != preset.modelFile) {
        throw std::runtime_error("Preset expects model '" + preset.modelFile + "', but received '" +
                                 modelPath.filename().string() + "'.");
    }

    OpenSim::Model model{modelPath.string()};
    model.finalizeConnections();
    SimTK::State& state = model.initSystem();
    SeatedMoCap::applyPosePreset(model, state, preset);

    const double maximumError = SeatedMoCap::verifyModelReferenceOrientations(model, state, preset);
    constexpr double toleranceDegrees = 0.01;

    if (maximumError > toleranceDegrees) {
        throw std::runtime_error("Written " + poseName + " preset has maximum error " +
                                 std::to_string(maximumError) + " degrees; expected at most " +
                                 std::to_string(toleranceDegrees) + " degrees.");
    }

    std::cout << "Verified written " << poseName << " preset; maximum error: " << maximumError
              << " degrees.\n";
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc != 3 && argc != 4) {
            std::cerr << "Usage:\n"
                      << "  generate_pose_presets "
                      << "<generated-model.osim> "
                      << "<output-pose-presets.json> "
                      << "[geometry-directory]\n";

            return 2;
        }

        const std::filesystem::path modelPath{argv[1]};

        const std::filesystem::path outputPath{argv[2]};

        if (!std::filesystem::is_regular_file(modelPath)) {

            throw std::runtime_error("Model file does not exist: " + modelPath.string());
        }

        if (argc == 4) {
            const std::filesystem::path geometryPath{argv[3]};

            if (!std::filesystem::is_directory(geometryPath)) {

                throw std::runtime_error("Geometry directory does not exist: " +
                                         geometryPath.string());
            }

            OpenSim::ModelVisualizer::addDirToGeometrySearchPaths(geometryPath.string());
        }

        // Candidate value to verify visually.
        // The right and left radioulnar joints are mirrored in the
        // Rajagopal model, so the same positive coordinate should
        // rotate the palms symmetrically.
        const double palmsTogether = 0.5 * SimTK::Pi;

        const PoseDefinition chairPalmsTogether{
            "chair_palms_together", 0.0,           0.5 * SimTK::Pi, 0.5 * SimTK::Pi,
            0.5 * SimTK::Pi,        palmsTogether, palmsTogether,   buildChairPoseTargets()};

        const PoseDefinition bedPalmsTogether{
            "bed_palms_together", 0.0,           0.5 * SimTK::Pi, 0.0,
            0.5 * SimTK::Pi,      palmsTogether, palmsTogether,   buildBedPoseTargets()};

        Json output;

        // Schema 2 distinguishes model-achievable calibration
        // orientations from ideal anatomical evaluation targets.
        output["schema_version"] = 2;
        output["model"] = modelPath.filename().string();
        output["quaternion_order"] = "wxyz";
        output["source_frame"] = "C";
        output["target_frame"] = "O";

        output["poses"]["chair_palms_together"] = generatePose(modelPath, chairPalmsTogether);

        output["poses"]["bed_palms_together"] = generatePose(modelPath, bedPalmsTogether);

        const auto parentDirectory = outputPath.parent_path();

        if (!parentDirectory.empty()) {
            std::filesystem::create_directories(parentDirectory);
        }

        std::ofstream outputFile{outputPath};

        if (!outputFile) {
            throw std::runtime_error("Could not open output file: " + outputPath.string());
        }

        outputFile << output.dump(4) << '\n';

        outputFile.close();
        if (!outputFile) {
            throw std::runtime_error("Could not finish writing output file: " +
                                     outputPath.string());
        }

        verifyWrittenPosePreset(modelPath, outputPath, "chair_palms_together");
        verifyWrittenPosePreset(modelPath, outputPath, "bed_palms_together");

        std::cout << "\nPose presets written to:\n"
                  << std::filesystem::absolute(outputPath).string() << '\n';

        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Pose preset generation failed: " << error.what() << '\n';

        return 1;
    }
}
