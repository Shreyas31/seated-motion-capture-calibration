#pragma once

namespace OpenSim {
class Model;
}

namespace SeatedMoCap {

/**
 * Applies the project's locked, unlocked, clamped, and palm-range coordinate policy.
 * @param model Augmented Rajagopal model to configure before serialization or IK use.
 * @throws std::runtime_error if any required model coordinate is missing.
 */
void configureCoordinatesForOrientationIk(OpenSim::Model& model);

} // namespace SeatedMoCap
