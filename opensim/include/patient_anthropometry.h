#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace SeatedMoCap {

/**
 * Stores validated clinician-supplied measurements used to scale the generic model.
 * @struct PatientAnthropometry.
 */
struct PatientAnthropometry {
    int schemaVersion{};
    std::string subjectId;
    double height{};
    std::optional<double> footLength;
    std::optional<double> massKg;

    /**
     * Loads schema-version-1 anthropometry expressed in metres and kilograms.
     * @param path Path to the patient anthropometry JSON file.
     * @return Parsed and validated patient measurements.
     * @throws std::runtime_error if the file, schema, units, fields, or ranges are invalid.
     */
    static PatientAnthropometry load(const std::filesystem::path& path);

    /**
     * Validates schema, subject ID, and clinically plausible measurement ranges.
     * @throws std::runtime_error if any required value is absent, non-finite, or out of range.
     */
    void validate() const;
};

/**
 * Stores dimensionless scale factors relative to the generic Rajagopal source model.
 * @struct AnthropometricScaleFactors.
 */
struct AnthropometricScaleFactors {
    double heightScale{};
    std::optional<double> footScale;
};

/**
 * Calculates scale factors relative to 1.70 m height and 0.25 m reference foot length.
 * @param patient Validated patient anthropometry.
 * @return Whole-body height scale and optional independent foot-length scale.
 */
AnthropometricScaleFactors calculateScaleFactors(const PatientAnthropometry& patient);

} // namespace SeatedMoCap
