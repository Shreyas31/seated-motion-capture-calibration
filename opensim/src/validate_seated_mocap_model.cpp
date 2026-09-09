#include "segment_model_map.h"

#include <OpenSim/OpenSim.h>
#include <cmath>
#include <exception>
#include <filesystem>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

class ValidationResults {
  public:
    void pass(const std::string& message) {
        std::cout << "[PASS] " << message << '\n';
        ++passed_;
    }

    void warning(const std::string& message) {
        std::cout << "[WARN] " << message << '\n';
        ++warnings_;
    }

    void fail(const std::string& message) {
        std::cerr << "[FAIL] " << message << '\n';
        ++failed_;
    }

    [[nodiscard]] bool successful() const {
        return failed_ == 0;
    }

    void printSummary() const {
        std::cout << "\nValidation summary\n"
                  << "  Passed:   " << passed_ << '\n'
                  << "  Warnings: " << warnings_ << '\n'
                  << "  Failed:   " << failed_ << '\n';
    }

  private:
    int passed_{};
    int warnings_{};
    int failed_{};
};

bool finiteVector(const SimTK::Vec3& vector) {
    return std::isfinite(vector[0]) && std::isfinite(vector[1]) && std::isfinite(vector[2]);
}

const OpenSim::Body* findBody(const OpenSim::Model& model, const std::string& name) {
    const auto& bodies = model.getBodySet();

    for (int index = 0; index < bodies.getSize(); ++index) {
        if (bodies.get(index).getName() == name) {
            return &bodies.get(index);
        }
    }

    return nullptr;
}

const OpenSim::PhysicalOffsetFrame* findUniqueFrame(const OpenSim::Model& model,
                                                    const std::string& name, int& matchCount) {
    const OpenSim::PhysicalOffsetFrame* result = nullptr;
    matchCount = 0;

    for (const auto& frame : model.getComponentList<OpenSim::PhysicalOffsetFrame>()) {

        if (frame.getName() == name) {
            result = &frame;
            ++matchCount;
        }
    }

    return result;
}

const OpenSim::Joint* findJoint(const OpenSim::Model& model, const std::string& name) {
    const auto& joints = model.getJointSet();

    for (int index = 0; index < joints.getSize(); ++index) {
        if (joints.get(index).getName() == name) {
            return &joints.get(index);
        }
    }

    return nullptr;
}

const OpenSim::Coordinate* findCoordinate(const OpenSim::Model& model, const std::string& name) {
    const auto& coordinates = model.getCoordinateSet();

    for (int index = 0; index < coordinates.getSize(); ++index) {

        if (coordinates.get(index).getName() == name) {
            return &coordinates.get(index);
        }
    }

    return nullptr;
}

void validateMapping(const OpenSim::Model& model, ValidationResults& results) {
    std::set<std::string> segments;
    std::set<std::string> frameNames;

    for (const auto& mapping : SeatedMoCap::kSegmentModelMappings) {

        const std::string segment{mapping.backendSegment};
        const std::string bodyName{mapping.opensimBody};
        const std::string frameName{mapping.imuFrame};

        if (!segments.insert(segment).second) {
            results.fail("Duplicate backend segment: " + segment);
        }

        if (!frameNames.insert(frameName).second) {
            results.fail("Duplicate mapping frame: " + frameName);
        }

        const OpenSim::Body* body = findBody(model, bodyName);

        if (body == nullptr) {
            results.fail(segment + " body is missing: " + bodyName);
            continue;
        }

        results.pass(segment + " body exists: " + bodyName);

        int frameMatches = 0;

        const OpenSim::PhysicalOffsetFrame* frame = findUniqueFrame(model, frameName, frameMatches);

        if (frameMatches == 0) {
            results.fail("IMU frame is missing: " + frameName);
            continue;
        }

        if (frameMatches > 1) {
            results.fail("IMU frame name is not unique: " + frameName);

            continue;
        }

        results.pass("IMU frame exists uniquely: " + frameName);

        const std::string baseFrameName = frame->findBaseFrame().getName();

        if (baseFrameName != bodyName) {
            results.fail(frameName + " has base body '" + baseFrameName + "', expected '" +
                         bodyName + "'.");

            continue;
        }

        results.pass(frameName + " is attached to " + bodyName);

        if (!finiteVector(frame->get_translation()) || !finiteVector(frame->get_orientation())) {

            results.fail(frameName + " contains a non-finite transform.");
        } else {
            results.pass(frameName + " has a finite transform.");
        }
    }
}

void validateJointConnection(const OpenSim::Model& model, ValidationResults& results,
                             const std::string& jointName, const std::string& expectedParentBody,
                             const std::string& expectedChildBody) {
    const OpenSim::Joint* joint = findJoint(model, jointName);

    if (joint == nullptr) {
        results.fail("Joint is missing: " + jointName);

        return;
    }

    results.pass("Joint exists: " + jointName);

    const std::string parentBody = joint->getParentFrame().findBaseFrame().getName();

    const std::string childBody = joint->getChildFrame().findBaseFrame().getName();

    if (parentBody != expectedParentBody) {
        results.fail(jointName + " parent is '" + parentBody + "', expected '" +
                     expectedParentBody + "'.");
    } else {
        results.pass(jointName + " parent is " + expectedParentBody);
    }

    if (childBody != expectedChildBody) {
        results.fail(jointName + " child is '" + childBody + "', expected '" + expectedChildBody +
                     "'.");
    } else {
        results.pass(jointName + " child is " + expectedChildBody);
    }
}

void validateAugmentedHierarchy(const OpenSim::Model& model, ValidationResults& results) {
    validateJointConnection(model, results, "neck_mocap", "torso", "head_mocap");

    validateJointConnection(model, results, "scapulothoracic_r_mocap", "torso",
                            "shoulder_girdle_r_mocap");

    validateJointConnection(model, results, "scapulothoracic_l_mocap", "torso",
                            "shoulder_girdle_l_mocap");

    // Verify that the original shoulder joints were reparented.
    validateJointConnection(model, results, "acromial_r", "shoulder_girdle_r_mocap", "humerus_r");

    validateJointConnection(model, results, "acromial_l", "shoulder_girdle_l_mocap", "humerus_l");
}

void validateNewCoordinates(const OpenSim::Model& model, ValidationResults& results) {
    const std::vector<std::string> expectedCoordinates{"neck_lateral_bending_mocap",
                                                       "neck_rotation_mocap",
                                                       "neck_flexion_mocap",

                                                       "shoulder_girdle_r_elevation",
                                                       "shoulder_girdle_r_protraction",
                                                       "shoulder_girdle_r_upward_rotation",

                                                       "shoulder_girdle_l_elevation",
                                                       "shoulder_girdle_l_protraction",
                                                       "shoulder_girdle_l_upward_rotation"};

    for (const std::string& name : expectedCoordinates) {
        const OpenSim::Coordinate* coordinate = findCoordinate(model, name);

        if (coordinate == nullptr) {
            results.fail("Coordinate is missing: " + name);

            continue;
        }

        results.pass("Coordinate exists: " + name);

        if (coordinate->get_locked()) {
            results.fail("New coordinate is unexpectedly locked: " + name);
        } else {
            results.pass("Coordinate is unlocked: " + name);
        }

        const double minimum = coordinate->getRangeMin();

        const double maximum = coordinate->getRangeMax();

        if (!std::isfinite(minimum) || !std::isfinite(maximum) || minimum >= maximum) {

            results.fail("Coordinate has an invalid range: " + name);
        } else {
            results.pass("Coordinate has a valid range: " + name);
        }
    }
}

void validateCoordinatePolicy(const OpenSim::Model& model, ValidationResults& results) {
    const std::vector<std::string> expectedLocked{"pelvis_tx", "pelvis_ty", "pelvis_tz",
                                                  "mtp_angle_r", "mtp_angle_l"};

    const std::vector<std::string> expectedUnlocked{"pelvis_tilt",
                                                    "pelvis_list",
                                                    "pelvis_rotation",

                                                    "hip_flexion_r",
                                                    "hip_adduction_r",
                                                    "hip_rotation_r",
                                                    "knee_angle_r",
                                                    "ankle_angle_r",
                                                    "subtalar_angle_r",

                                                    "hip_flexion_l",
                                                    "hip_adduction_l",
                                                    "hip_rotation_l",
                                                    "knee_angle_l",
                                                    "ankle_angle_l",
                                                    "subtalar_angle_l",

                                                    "lumbar_extension",
                                                    "lumbar_bending",
                                                    "lumbar_rotation",

                                                    "arm_flex_r",
                                                    "arm_add_r",
                                                    "arm_rot_r",
                                                    "elbow_flex_r",
                                                    "pro_sup_r",
                                                    "wrist_flex_r",
                                                    "wrist_dev_r",

                                                    "arm_flex_l",
                                                    "arm_add_l",
                                                    "arm_rot_l",
                                                    "elbow_flex_l",
                                                    "pro_sup_l",
                                                    "wrist_flex_l",
                                                    "wrist_dev_l",

                                                    "shoulder_girdle_r_elevation",
                                                    "shoulder_girdle_r_protraction",
                                                    "shoulder_girdle_r_upward_rotation",

                                                    "shoulder_girdle_l_elevation",
                                                    "shoulder_girdle_l_protraction",
                                                    "shoulder_girdle_l_upward_rotation",

                                                    "neck_lateral_bending_mocap",
                                                    "neck_rotation_mocap",
                                                    "neck_flexion_mocap"};

    for (const std::string& name : expectedLocked) {
        const OpenSim::Coordinate* coordinate = findCoordinate(model, name);

        if (coordinate == nullptr) {
            results.fail("Policy coordinate is missing: " + name);

            continue;
        }

        if (!coordinate->get_locked()) {
            results.fail("Coordinate should be locked: " + name);
        } else {
            results.pass("Coordinate is correctly locked: " + name);
        }
    }

    for (const std::string& name : expectedUnlocked) {
        const OpenSim::Coordinate* coordinate = findCoordinate(model, name);

        if (coordinate == nullptr) {
            results.fail("Policy coordinate is missing: " + name);

            continue;
        }

        if (coordinate->get_locked()) {
            results.fail("Coordinate should be unlocked: " + name);
        } else {
            results.pass("Coordinate is correctly unlocked: " + name);
        }
    }

    for (const char* name : {"pro_sup_r", "pro_sup_l"}) {

        const OpenSim::Coordinate* coordinate = findCoordinate(model, name);

        if (coordinate == nullptr) {
            results.fail("Palm-rotation coordinate is missing: " + std::string{name});

            continue;
        }

        constexpr double tolerance = 1e-8;

        if (std::abs(coordinate->getRangeMin()) > tolerance ||
            std::abs(coordinate->getRangeMax() - SimTK::Pi) > tolerance) {
            results.fail("Palm-rotation range should be 0 to pi: " + std::string{name});
        } else {
            results.pass("Palm-rotation range is 0 to pi: " + std::string{name});
        }
    }
}

void validateModelState(OpenSim::Model& model, ValidationResults& results) {
    try {
        SimTK::State& state = model.initSystem();

        results.pass("OpenSim system initialized successfully.");

        for (const auto& coordinate : model.getComponentList<OpenSim::Coordinate>()) {

            const double value = coordinate.getValue(state);

            if (!std::isfinite(value)) {
                results.fail("Coordinate state is non-finite: " + coordinate.getName());
            }
        }

        results.pass("All initial coordinate values are finite.");
    } catch (const std::exception& error) {
        results.fail(std::string{"Model system initialization failed: "} + error.what());
    }
}

void validateMassProperties(const OpenSim::Model& model, ValidationResults& results) {
    double totalMass = 0.0;

    for (int index = 0; index < model.getBodySet().getSize(); ++index) {

        const auto& body = model.getBodySet().get(index);

        const double mass = body.getMass();

        if (!std::isfinite(mass) || mass <= 0.0) {
            results.fail("Body has invalid mass: " + body.getName());

            continue;
        }

        totalMass += mass;
    }

    if (!std::isfinite(totalMass) || totalMass <= 0.0) {

        results.fail("Model total mass is invalid.");
    } else {
        results.pass("All body masses are positive and finite.");

        std::cout << "[INFO] Model total mass: " << totalMass << " kg\n";
    }

    results.warning("The three augmented bodies currently use "
                    "placeholder inertial properties; this model "
                    "is kinematic-only.");
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc != 2 && argc != 3) {
            std::cerr << "Usage:\n"
                      << "  validate_seated_mocap_model "
                      << "<generated-model.osim> "
                      << "[geometry-directory]\n";

            return 2;
        }

        const std::filesystem::path modelPath{argv[1]};

        if (!std::filesystem::is_regular_file(modelPath)) {
            throw std::runtime_error("Model file does not exist: " + modelPath.string());
        }

        if (argc == 3) {
            const std::filesystem::path geometryPath{argv[2]};

            if (!std::filesystem::is_directory(geometryPath)) {

                throw std::runtime_error("Geometry directory does not exist: " +
                                         geometryPath.string());
            }

            OpenSim::ModelVisualizer::addDirToGeometrySearchPaths(geometryPath.string());
        }

        OpenSim::Model model{modelPath.string()};
        model.finalizeConnections();

        ValidationResults results;

        validateMapping(model, results);
        validateAugmentedHierarchy(model, results);
        validateNewCoordinates(model, results);
        validateCoordinatePolicy(model, results);
        validateMassProperties(model, results);
        validateModelState(model, results);

        results.printSummary();

        return results.successful() ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "Validation could not run: " << error.what() << '\n';

        return 1;
    }
}
