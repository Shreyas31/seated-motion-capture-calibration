#include "support_plane_alignment.h"

#include <OpenSim/OpenSim.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

constexpr double kReferenceSoleY = -0.009839;
constexpr double kReferenceHeelX = -0.005;
constexpr double kReferenceForefootX = 0.175;
constexpr double kReferenceMedialLateralZ = 0.035;

constexpr std::array<const char*, 8> kSoleLandmarkNames{
    "sole_r_heel_medial",      "sole_r_heel_lateral",    "sole_r_forefoot_medial",
    "sole_r_forefoot_lateral", "sole_l_heel_medial",     "sole_l_heel_lateral",
    "sole_l_forefoot_medial",  "sole_l_forefoot_lateral"};

constexpr std::array<const char*, 4> kRightSoleLandmarkNames{
    "sole_r_heel_medial", "sole_r_heel_lateral", "sole_r_forefoot_medial",
    "sole_r_forefoot_lateral"};

constexpr std::array<const char*, 4> kLeftSoleLandmarkNames{
    "sole_l_heel_medial", "sole_l_heel_lateral", "sole_l_forefoot_medial",
    "sole_l_forefoot_lateral"};

OpenSim::Coordinate& requireCoordinate(OpenSim::Model& model, const std::string& name) {
    auto& coordinates = model.updCoordinateSet();

    for (int index = 0; index < coordinates.getSize(); ++index) {

        auto& coordinate = coordinates.get(index);

        if (coordinate.getName() == name) {
            return coordinate;
        }
    }

    throw std::runtime_error("Required coordinate is missing: " + name);
}

void addLandmark(OpenSim::Model& model, const OpenSim::PhysicalFrame& frame,
                 const std::string& name, const SimTK::Vec3& location) {
    auto* station = new OpenSim::Station{frame, location};

    station->setName(name);
    model.addComponent(station);
}

const OpenSim::Station& requireLandmark(const OpenSim::Model& model, const std::string& name) {
    for (const auto& station : model.getComponentList<OpenSim::Station>()) {

        if (station.getName() == name) {
            return station;
        }
    }

    throw std::runtime_error("Required foot-sole landmark is missing: " + name);
}

template <std::size_t Size>
SimTK::Vec3 calculateLandmarkCentroid(const OpenSim::Model& model, const SimTK::State& state,
                                      const std::array<const char*, Size>& names) {
    SimTK::Vec3 centroid{0.0};

    for (const char* name : names) {
        centroid += requireLandmark(model, name).getLocationInGround(state);
    }

    return centroid / static_cast<double>(Size);
}

std::array<double, 3> toArray(const SimTK::Vec3& value) {
    return {value[0], value[1], value[2]};
}

SimTK::Vec3 toVec3(const std::array<double, 3>& value) {
    return SimTK::Vec3{value[0], value[1], value[2]};
}

void applyPelvisTranslationCorrection(OpenSim::Model& model, SimTK::State& state,
                                      const SimTK::Vec3& correction) {
    constexpr std::array<const char*, 3> coordinateNames{"pelvis_tx", "pelvis_ty", "pelvis_tz"};

    std::array<OpenSim::Coordinate*, 3> coordinates{};

    std::array<bool, 3> previouslyLocked{};

    for (std::size_t index = 0; index < coordinateNames.size(); ++index) {

        coordinates[index] = &requireCoordinate(model, coordinateNames[index]);

        previouslyLocked[index] = coordinates[index]->getLocked(state);

        if (previouslyLocked[index]) {
            coordinates[index]->setLocked(state, false);
        }
    }

    for (std::size_t index = 0; index < coordinates.size(); ++index) {

        coordinates[index]->setValue(
            state, coordinates[index]->getValue(state) + correction[static_cast<int>(index)],
            false);
    }

    for (std::size_t index = 0; index < coordinates.size(); ++index) {

        if (previouslyLocked[index]) {
            coordinates[index]->setLocked(state, true);
        }
    }

    model.realizePosition(state);
}

} // namespace

namespace SeatedMoCap {

void addFootSoleLandmarks(OpenSim::Model& model, double footScale) {
    if (!std::isfinite(footScale) || footScale <= 0.0) {

        throw std::runtime_error("Foot scale must be finite and positive.");
    }

    const auto& rightFoot = model.getBodySet().get("calcn_r");

    const auto& leftFoot = model.getBodySet().get("calcn_l");

    const double heelX = kReferenceHeelX * footScale;

    const double forefootX = kReferenceForefootX * footScale;

    const double soleY = kReferenceSoleY * footScale;

    const double lateralZ = kReferenceMedialLateralZ * footScale;

    // The four points approximate the underside of the
    // Rajagopal r_foot/l_foot meshes. Using both heel and
    // forefoot points also supports the bed pose, where the
    // foot may not be flat relative to ground.
    addLandmark(model, rightFoot, "sole_r_heel_medial", SimTK::Vec3{heelX, soleY, -lateralZ});

    addLandmark(model, rightFoot, "sole_r_heel_lateral", SimTK::Vec3{heelX, soleY, lateralZ});

    addLandmark(model, rightFoot, "sole_r_forefoot_medial",
                SimTK::Vec3{forefootX, soleY, -lateralZ});

    addLandmark(model, rightFoot, "sole_r_forefoot_lateral",
                SimTK::Vec3{forefootX, soleY, lateralZ});

    addLandmark(model, leftFoot, "sole_l_heel_medial", SimTK::Vec3{heelX, soleY, lateralZ});

    addLandmark(model, leftFoot, "sole_l_heel_lateral", SimTK::Vec3{heelX, soleY, -lateralZ});

    addLandmark(model, leftFoot, "sole_l_forefoot_medial", SimTK::Vec3{forefootX, soleY, lateralZ});

    addLandmark(model, leftFoot, "sole_l_forefoot_lateral",
                SimTK::Vec3{forefootX, soleY, -lateralZ});
}

double alignFeetToSupportPlane(OpenSim::Model& model, SimTK::State& state,
                               double supportHeightMetres) {
    if (!std::isfinite(supportHeightMetres)) {

        throw std::runtime_error("Support-plane height must be finite.");
    }

    model.realizePosition(state);

    double minimumHeight = std::numeric_limits<double>::infinity();

    double rightMinimum = std::numeric_limits<double>::infinity();

    double leftMinimum = std::numeric_limits<double>::infinity();

    for (const char* name : kSoleLandmarkNames) {

        const auto& station = requireLandmark(model, name);

        const SimTK::Vec3 location = station.getLocationInGround(state);

        minimumHeight = (std::min)(minimumHeight, location[1]);

        if (std::string{name}.find("_r_") != std::string::npos) {

            rightMinimum = (std::min)(rightMinimum, location[1]);
        } else {
            leftMinimum = (std::min)(leftMinimum, location[1]);
        }
    }

    if (!std::isfinite(minimumHeight)) {
        throw std::runtime_error("Could not calculate foot-sole height.");
    }

    const double verticalCorrection = supportHeightMetres - minimumHeight;

    auto& pelvisTy = requireCoordinate(model, "pelvis_ty");

    const bool wasLocked = pelvisTy.getLocked(state);

    if (wasLocked) {
        pelvisTy.setLocked(state, false);
    }

    pelvisTy.setValue(state, pelvisTy.getValue(state) + verticalCorrection, false);

    if (wasLocked) {
        pelvisTy.setLocked(state, true);
    }

    model.realizePosition(state);

    const double leftRightDifference = std::abs(leftMinimum - rightMinimum);

    std::cout << "Support-plane alignment:\n"
              << "  Initial lowest sole point: " << minimumHeight << " m\n"
              << "  Applied pelvis_ty correction: " << verticalCorrection << " m\n"
              << "  Support-plane height: " << supportHeightMetres << " m\n"
              << "  Left/right sole-height difference: " << leftRightDifference << " m\n";

    if (leftRightDifference > 0.01) {
        std::cerr << "WARNING: Left and right feet differ in "
                  << "height by more than 10 mm." << std::endl;
    }

    return verticalCorrection;
}

StationaryFootAnchor::StationaryFootAnchor(const OpenSim::Model& model, const SimTK::State& state) {
    model.realizePosition(state);

    initialRight_ = toArray(calculateLandmarkCentroid(model, state, kRightSoleLandmarkNames));

    initialLeft_ = toArray(calculateLandmarkCentroid(model, state, kLeftSoleLandmarkNames));

    const SimTK::Vec3 right = toVec3(initialRight_);

    const SimTK::Vec3 left = toVec3(initialLeft_);

    std::cout << "Chair stationary-foot anchors captured:\n"
              << "  Right sole centroid: " << right << " m\n"
              << "  Left sole centroid: " << left << " m\n";
}

FootAnchorUpdate StationaryFootAnchor::apply(OpenSim::Model& model, SimTK::State& state) const {
    model.realizePosition(state);

    const SimTK::Vec3 currentRight =
        calculateLandmarkCentroid(model, state, kRightSoleLandmarkNames);

    const SimTK::Vec3 currentLeft = calculateLandmarkCentroid(model, state, kLeftSoleLandmarkNames);

    const SimTK::Vec3 initialRight = toVec3(initialRight_);

    const SimTK::Vec3 initialLeft = toVec3(initialLeft_);

    const SimTK::Vec3 initialMean = 0.5 * (initialRight + initialLeft);

    const SimTK::Vec3 currentMean = 0.5 * (currentRight + currentLeft);

    // One common root translation can preserve the mean foot location.
    // It cannot independently correct two inconsistent feet.
    const SimTK::Vec3 correction = initialMean - currentMean;

    applyPelvisTranslationCorrection(model, state, correction);

    const SimTK::Vec3 correctedRight = currentRight + correction;

    const SimTK::Vec3 correctedLeft = currentLeft + correction;

    const double rightResidual = (correctedRight - initialRight).norm();

    const double leftResidual = (correctedLeft - initialLeft).norm();

    const double maximumResidual = (std::max)(rightResidual, leftResidual);

    auto& pelvisTx = requireCoordinate(model, "pelvis_tx");

    auto& pelvisTy = requireCoordinate(model, "pelvis_ty");

    auto& pelvisTz = requireCoordinate(model, "pelvis_tz");

    FootAnchorUpdate result;

    result.correctionX = correction[0];
    result.correctionY = correction[1];
    result.correctionZ = correction[2];

    result.pelvisX = pelvisTx.getValue(state);

    result.pelvisY = pelvisTy.getValue(state);

    result.pelvisZ = pelvisTz.getValue(state);

    result.leftResidual = leftResidual;

    result.rightResidual = rightResidual;

    result.maximumResidual = maximumResidual;

    return result;
}

} // namespace SeatedMoCap
