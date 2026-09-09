#pragma once

namespace OpenSim {
class Model;
}

namespace SeatedMoCap {

/**
 * Adds the project-specific head, neck, and bilateral shoulder-girdle proxy bodies and joints.
 * @param model Rajagopal model to augment before finalizing its connections.
 * @param heightScale Whole-body height scale relative to the 1.70 m source model.
 * @throws std::invalid_argument if heightScale is non-finite or non-positive.
 * @throws std::runtime_error if required Rajagopal parent bodies are unavailable.
 */
void augmentRajagopalUpperBody(OpenSim::Model& model, double heightScale);

} // namespace SeatedMoCap
