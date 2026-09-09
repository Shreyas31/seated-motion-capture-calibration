#pragma once

namespace OpenSim {
class Model;
}

namespace SeatedMoCap {

/**
 * Align every virtual IMU frame with the project's anatomical
 * segment convention using Rajagopal's neutral standing model state.
 *
 * This is an offline model-generation operation. It does not require
 * the patient to stand.
 *
 * Only fixed virtual-frame orientations are changed. Joint axes,
 * geometry and model coordinates are not modified.
 * @param model Augmented Rajagopal model whose 17 virtual IMU frames are updated.
 * @throws std::runtime_error if a required frame, body, or neutral coordinate is missing.
 */
void alignAllVirtualImuFrames(OpenSim::Model& model);

} // namespace SeatedMoCap
