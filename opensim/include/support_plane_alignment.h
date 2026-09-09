#pragma once

#include <array>

namespace OpenSim {
class Model;
}

namespace SimTK {
class State;
}

namespace SeatedMoCap {

/**
 * Adds scaled sole landmarks to both calcaneus bodies.
 * @param model Model receiving heel and forefoot station landmarks.
 * @param footScale Dimensionless foot-length scale relative to the source model.
 * @throws std::runtime_error if either required calcaneus body is missing.
 */
void addFootSoleLandmarks(OpenSim::Model& model, double footScale);

/**
 * Translates the pelvis vertically until the lowest sole landmark
 * reaches supportHeightMetres.
 * @param model Model containing pelvis translation and sole landmarks.
 * @param state Mutable model state receiving the vertical correction.
 * @param supportHeightMetres Desired support-plane height in OpenSim ground coordinates.
 * @return Applied pelvis vertical translation in metres.
 * @throws std::runtime_error if required coordinates or sole landmarks are unavailable.
 */
double alignFeetToSupportPlane(OpenSim::Model& model, SimTK::State& state,
                               double supportHeightMetres = 0.0);

/**
 * Reports the pelvis translation and bilateral residuals from one stationary-foot update.
 * @struct FootAnchorUpdate.
 */
struct FootAnchorUpdate {
    double correctionX{};
    double correctionY{};
    double correctionZ{};

    double pelvisX{};
    double pelvisY{};
    double pelvisZ{};

    double leftResidual{};
    double rightResidual{};
    double maximumResidual{};
};

/**
 * Captures the initial left and right foot-sole centroids and subsequently
 * maintains their average position using only the model's global pelvis
 * translation.
 *
 * This is a kinematic estimate under a stationary-feet assumption. It does
 * not measure global translation directly.
 * @class StationaryFootAnchor.
 */
class StationaryFootAnchor {
  public:
    /**
     * Captures initial left and right sole-centroid positions.
     * @param model Model containing the sole landmark stations.
     * @param state Initial seated state used as the stationary reference.
     * @throws std::runtime_error if required landmarks or pelvis translations are missing.
     */
    StationaryFootAnchor(const OpenSim::Model& model, const SimTK::State& state);

    /**
     * Corrects global pelvis translation to preserve the mean initial foot position.
     * @param model Model whose pelvis translation coordinates are adjusted.
     * @param state Current IK state receiving the correction.
     * @return Corrections, resulting pelvis position, and foot residuals in metres.
     * @throws std::runtime_error if required coordinates or landmarks are unavailable.
     */
    FootAnchorUpdate apply(OpenSim::Model& model, SimTK::State& state) const;

  private:
    std::array<double, 3> initialLeft_{};
    std::array<double, 3> initialRight_{};
};
} // namespace SeatedMoCap
