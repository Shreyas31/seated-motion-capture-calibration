#include "pose_preset.h"

#include "segment_model_map.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <set>
#include <stdexcept>
#include <vector>

namespace {

using Json = nlohmann::json;

bool isDependentKneeCoordinate(const std::string& name) {
    return name == "knee_angle_r_beta" || name == "knee_angle_l_beta";
}

std::map<std::string, double> readScalarMap(const Json& object, const std::string& fieldName) {
    if (!object.is_object()) {
        throw std::runtime_error("'" + fieldName + "' must be a JSON object.");
    }

    std::map<std::string, double> result;

    for (auto iterator = object.begin(); iterator != object.end(); ++iterator) {

        const double value = iterator.value().get<double>();

        if (!std::isfinite(value)) {
            throw std::runtime_error("Non-finite value in '" + fieldName + "': " + iterator.key());
        }

        result.emplace(iterator.key(), value);
    }

    return result;
}

std::map<std::string, std::array<double, 4>> readQuaternionMap(const Json& object,
                                                               const std::string& fieldName) {
    if (!object.is_object()) {
        throw std::runtime_error("'" + fieldName + "' must be a JSON object.");
    }

    std::map<std::string, std::array<double, 4>> result;

    for (auto iterator = object.begin(); iterator != object.end(); ++iterator) {

        const Json& quaternion = iterator.value();

        if (!quaternion.is_array() || quaternion.size() != 4) {

            throw std::runtime_error("Quaternion must contain four values: " + iterator.key());
        }

        std::array<double, 4> values{quaternion.at(0).get<double>(), quaternion.at(1).get<double>(),
                                     quaternion.at(2).get<double>(),
                                     quaternion.at(3).get<double>()};

        double normSquared = 0.0;

        for (double value : values) {
            if (!std::isfinite(value)) {
                throw std::runtime_error("Quaternion contains a non-finite value: " +
                                         iterator.key());
            }

            normSquared += value * value;
        }

        const double norm = std::sqrt(normSquared);

        if (std::abs(norm - 1.0) > 1e-6) {
            throw std::runtime_error("Quaternion is not normalised: " + iterator.key());
        }

        result.emplace(iterator.key(), values);
    }

    return result;
}

const OpenSim::PhysicalOffsetFrame& requireFrame(const OpenSim::Model& model,
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
        throw std::runtime_error("Model reference frame is missing: " + name);
    }

    if (matches > 1) {
        throw std::runtime_error("Model reference frame is not unique: " + name);
    }

    return *result;
}

double rotationErrorDegrees(const SimTK::Rotation& expected, const SimTK::Rotation& actual) {
    const SimTK::Rotation difference = (~expected) * actual;

    const SimTK::Vec4 angleAxis = difference.convertRotationToAngleAxis();

    return std::abs(angleAxis[0]) * (180.0 / SimTK::Pi);
}

} // namespace

namespace SeatedMoCap {

PosePreset loadPosePreset(const std::filesystem::path& jsonPath, const std::string& poseName) {
    if (!std::filesystem::is_regular_file(jsonPath)) {

        throw std::runtime_error("Pose preset file does not exist: " + jsonPath.string());
    }

    std::ifstream input{jsonPath};

    if (!input) {
        throw std::runtime_error("Could not open pose preset file: " + jsonPath.string());
    }

    Json root;
    input >> root;

    if (root.at("schema_version").get<int>() != 2) {
        throw std::runtime_error("Unsupported pose-preset schema version.");
    }

    if (root.at("quaternion_order").get<std::string>() != "wxyz") {

        throw std::runtime_error("Pose presets must use wxyz quaternion order.");
    }

    if (root.at("source_frame").get<std::string>() != "C" ||
        root.at("target_frame").get<std::string>() != "O") {

        throw std::runtime_error("Unexpected pose-preset coordinate frames.");
    }

    if (poseName != "chair_palms_together" && poseName != "bed_palms_together") {

        throw std::runtime_error("Unsupported pose preset: " + poseName);
    }

    const Json& pose = root.at("poses").at(poseName);

    if (pose.at("generation_method").get<std::string>() !=
        "prescribed_coordinates_forward_kinematics") {

        throw std::runtime_error("Pose was not generated using forward kinematics.");
    }

    PosePreset result;

    result.name = poseName;
    result.modelFile = root.at("model").get<std::string>();

    result.coordinatesRad = readScalarMap(pose.at("coordinates_rad"), "coordinates_rad");

    result.coordinatesM = readScalarMap(pose.at("coordinates_m"), "coordinates_m");

    result.modelReferenceOrientationsWxyz = readQuaternionMap(
        pose.at("model_reference_orientations_wxyz"), "model_reference_orientations_wxyz");

    result.idealOrientationsWxyz =
        readQuaternionMap(pose.at("ideal_orientations_wxyz"), "ideal_orientations_wxyz");

    result.modelRepresentationErrorDegrees =
        readScalarMap(pose.at("model_representation_error_deg"), "model_representation_error_deg");

    if (result.modelReferenceOrientationsWxyz.size() != kSegmentModelMappings.size()) {

        throw std::runtime_error("Pose preset does not contain " +
                                 std::to_string(kSegmentModelMappings.size()) +
                                 " model reference orientations.");
    }

    for (const auto& mapping : kSegmentModelMappings) {

        const std::string frameName{mapping.imuFrame};

        if (result.modelReferenceOrientationsWxyz.find(frameName) ==
            result.modelReferenceOrientationsWxyz.end()) {

            throw std::runtime_error("Pose preset is missing frame: " + frameName);
        }
    }

    return result;
}

void applyPosePreset(OpenSim::Model& model, SimTK::State& state, const PosePreset& preset) {
    std::vector<OpenSim::Coordinate*> temporarilyLocked;

    std::set<std::string> processedPaths;

    const auto applyCoordinateMap = [&](const std::map<std::string, double>& values,
                                        OpenSim::Coordinate::MotionType expectedType) {
        for (const auto& [path, value] : values) {
            if (!processedPaths.insert(path).second) {
                throw std::runtime_error("Coordinate occurs more than once: " + path);
            }

            auto& coordinate = model.updComponent<OpenSim::Coordinate>(path);

            // Rajagopal's beta coordinates are coupled coordinates.
            // Their motion type is not reported as an ordinary rotational
            // coordinate, and their values are calculated during assembly.
            if (isDependentKneeCoordinate(coordinate.getName())) {

                continue;
            }

            if (coordinate.getMotionType() != expectedType) {

                throw std::runtime_error("Coordinate has unexpected units/type: " + path);
            }

            if (value < coordinate.getRangeMin() || value > coordinate.getRangeMax()) {

                throw std::runtime_error("Preset value is outside the range: " + path);
            }

            if (coordinate.getLocked(state)) {
                if (std::abs(coordinate.getValue(state) - value) > 1e-8) {

                    throw std::runtime_error("Locked coordinate does not match preset: " + path);
                }

                continue;
            }

            coordinate.setValue(state, value, false);

            coordinate.setLocked(state, true);

            temporarilyLocked.push_back(&coordinate);
        }
    };

    applyCoordinateMap(preset.coordinatesRad, OpenSim::Coordinate::MotionType::Rotational);

    applyCoordinateMap(preset.coordinatesM, OpenSim::Coordinate::MotionType::Translational);

    // Independent pose coordinates remain locked while the
    // coupled knee/patellofemoral coordinates are assembled.
    model.assemble(state);

    for (OpenSim::Coordinate* coordinate : temporarilyLocked) {

        coordinate->setLocked(state, false);
    }

    model.realizePosition(state);
}

double verifyModelReferenceOrientations(const OpenSim::Model& model, const SimTK::State& state,
                                        const PosePreset& preset) {
    double maximumError = 0.0;
    std::string worstFrame;

    std::cout << "\nModel-reference verification: " << preset.name << '\n';

    for (const auto& [frameName, values] : preset.modelReferenceOrientationsWxyz) {

        const SimTK::Quaternion quaternion{values[0], values[1], values[2], values[3]};

        const SimTK::Rotation expected{quaternion};

        const auto& frame = requireFrame(model, frameName);

        const SimTK::Rotation actual = frame.getTransformInGround(state).R();

        const double error = rotationErrorDegrees(expected, actual);

        std::cout << "  " << frameName << ": " << error << " degrees\n";

        if (error > maximumError) {
            maximumError = error;
            worstFrame = frameName;
        }
    }

    std::cout << "Maximum reference error: " << maximumError << " degrees";

    if (!worstFrame.empty()) {
        std::cout << " at " << worstFrame;
    }

    std::cout << '\n';

    return maximumError;
}

} // namespace SeatedMoCap
