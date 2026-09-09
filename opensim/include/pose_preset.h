#pragma once

#include <OpenSim/OpenSim.h>
#include <array>
#include <filesystem>
#include <map>
#include <string>

namespace SeatedMoCap {

/**
 * Stores one model-achievable reference pose and its ideal segment orientations.
 * @struct PosePreset.
 */
struct PosePreset {
    std::string name;
    std::string modelFile;

    std::map<std::string, double> coordinatesRad;
    std::map<std::string, double> coordinatesM;

    std::map<std::string, std::array<double, 4>> modelReferenceOrientationsWxyz;
    std::map<std::string, std::array<double, 4>> idealOrientationsWxyz;
    std::map<std::string, double> modelRepresentationErrorDegrees;
};

/**
 * Loads and validates one named pose from the pose-preset JSON document.
 * @param jsonPath Path to the generated pose-presets file.
 * @param poseName Pose key for chair or bed with its palm suffix.
 * @return Validated pose coordinates and frame orientations.
 * @throws std::runtime_error if the file, schema, pose, coordinates, or frames are invalid.
 */
PosePreset loadPosePreset(const std::filesystem::path& jsonPath, const std::string& poseName);

/**
 * Applies preset rotational and translational coordinate values to a model state.
 * @param model Model whose coordinate set defines the named coordinates.
 * @param state Mutable state receiving the preset values.
 * @param preset Validated pose preset to apply.
 * @throws std::runtime_error if a coordinate is missing or has an unexpected motion type.
 */
void applyPosePreset(OpenSim::Model& model, SimTK::State& state, const PosePreset& preset);

/**
 * Measures the largest orientation mismatch between model IMU frames and preset references.
 * @param model Model containing the required virtual IMU frames.
 * @param state Realized state representing the applied pose.
 * @param preset Preset containing model-reference frame orientations.
 * @return Maximum frame orientation error in degrees.
 * @throws std::runtime_error if a required frame or reference orientation is missing.
 */
double verifyModelReferenceOrientations(const OpenSim::Model& model, const SimTK::State& state,
                                        const PosePreset& preset);

} // namespace SeatedMoCap
