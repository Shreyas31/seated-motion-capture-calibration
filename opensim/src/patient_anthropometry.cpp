#include "patient_anthropometry.h"

#include <cmath>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

namespace {

constexpr double kRajagopalReferenceHeight = 1.70;

// Initial reference for the generic visual model.
// Foot-specific scaling should remain labelled approximate until the
// measurement definition has been validated against the model geometry.
constexpr double kRajagopalReferenceFootLength = 0.25;

std::optional<double> readOptionalDouble(const nlohmann::json& json, const char* name) {
    if (!json.contains(name) || json.at(name).is_null()) {
        return std::nullopt;
    }

    return json.at(name).get<double>();
}

void requireRange(const char* name, double value, double minimum, double maximum) {
    if (!std::isfinite(value) || value < minimum || value > maximum) {

        throw std::runtime_error(std::string{name} + " is outside the accepted range [" +
                                 std::to_string(minimum) + ", " + std::to_string(maximum) + "].");
    }
}

} // namespace

namespace SeatedMoCap {

PatientAnthropometry PatientAnthropometry::load(const std::filesystem::path& path) {
    std::ifstream input{path};

    if (!input) {
        throw std::runtime_error("Could not open patient anthropometry file: " + path.string());
    }

    nlohmann::json json;
    input >> json;

    if (json.at("units").get<std::string>() != "m") {
        throw std::runtime_error("Anthropometric lengths must use metres.");
    }

    PatientAnthropometry patient;

    patient.schemaVersion = json.at("schema_version").get<int>();

    patient.subjectId = json.at("subject_id").get<std::string>();

    patient.height = json.at("patient_height").get<double>();

    patient.footLength = readOptionalDouble(json, "foot_length");

    patient.massKg = readOptionalDouble(json, "patient_mass_kg");

    patient.validate();
    return patient;
}

void PatientAnthropometry::validate() const {
    if (schemaVersion != 1) {
        throw std::runtime_error("Unsupported anthropometry schema version: " +
                                 std::to_string(schemaVersion));
    }

    if (subjectId.empty()) {
        throw std::runtime_error("subject_id must not be empty.");
    }

    requireRange("patient_height", height, 1.0, 2.3);

    if (footLength) {
        requireRange("foot_length", *footLength, 0.15, 0.40);
    }

    if (massKg) {
        requireRange("patient_mass_kg", *massKg, 25.0, 300.0);
    }
}

AnthropometricScaleFactors calculateScaleFactors(const PatientAnthropometry& patient) {
    AnthropometricScaleFactors factors;

    factors.heightScale = patient.height / kRajagopalReferenceHeight;

    if (patient.footLength) {
        factors.footScale = *patient.footLength / kRajagopalReferenceFootLength;
    }

    return factors;
}

} // namespace SeatedMoCap
