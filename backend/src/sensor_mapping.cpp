#include "sensor_mapping.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <set>
#include <stdexcept>
#include <utility>

using json = nlohmann::json;

namespace {

const std::vector<std::string> CANONICAL_SEGMENTS = {"Right_Hand",
                                                     "Left_Hand",
                                                     "Right_Forearm",
                                                     "Left_Forearm",
                                                     "Right_Upperarm",
                                                     "Left_Upperarm",
                                                     "Right_Shoulder",
                                                     "Left_Shoulder",
                                                     "Right_Foot",
                                                     "Left_Foot",
                                                     "Right_Lowerleg",
                                                     "Left_Lowerleg",
                                                     "Right_Upperleg",
                                                     "Left_Upperleg",
                                                     "Pelvis",
                                                     "Sternum",
                                                     "Head"};

} // namespace

SensorMapping::SensorMapping(std::map<std::string, std::string> sensorToSegment)
    : sensorToSegment_(std::move(sensorToSegment)) {

    for (const auto& [sensorId, segment] : sensorToSegment_) {

        segmentToSensor_.emplace(segment, sensorId);
    }
}

SensorMapping SensorMapping::loadFromJson(const std::filesystem::path& filename) {

    std::ifstream input(filename);

    if (!input.is_open()) {
        throw std::runtime_error("Could not open sensor mapping file: " + filename.string());
    }

    json document;

    try {
        input >> document;
    } catch (const json::exception& error) {
        throw std::runtime_error("Could not parse sensor mapping JSON: " +
                                 std::string(error.what()));
    }

    if (!document.contains("schema_version") || !document["schema_version"].is_number_integer() ||
        document["schema_version"].get<int>() != 1) {
        throw std::runtime_error("Sensor mapping must use schema_version 1.");
    }

    if (!document.contains("sensors") || !document["sensors"].is_array()) {
        throw std::runtime_error("Sensor mapping must contain a sensors array.");
    }

    const std::set<std::string> recognisedSegments(CANONICAL_SEGMENTS.begin(),
                                                   CANONICAL_SEGMENTS.end());

    std::map<std::string, std::string> sensorToSegment;

    std::set<std::string> assignedSegments;

    for (const json& entry : document["sensors"]) {
        if (!entry.is_object() || !entry.contains("segment") || !entry.contains("sensor_id") ||
            !entry["segment"].is_string() || !entry["sensor_id"].is_string()) {
            throw std::runtime_error("Every sensor entry must contain string "
                                     "segment and sensor_id fields.");
        }

        const std::string segment = entry["segment"].get<std::string>();

        const std::string sensorId = entry["sensor_id"].get<std::string>();

        if (segment.empty() || sensorId.empty()) {
            throw std::runtime_error("Segment names and sensor IDs cannot be empty.");
        }

        if (!recognisedSegments.contains(segment)) {
            throw std::runtime_error("Unknown segment in sensor mapping: " + segment);
        }

        if (!assignedSegments.insert(segment).second) {
            throw std::runtime_error("Segment is assigned more than once: " + segment);
        }

        if (!sensorToSegment.emplace(sensorId, segment).second) {
            throw std::runtime_error("Sensor ID is assigned more than once: " + sensorId);
        }
    }

    if (assignedSegments.size() != CANONICAL_SEGMENTS.size()) {
        std::string missingSegments;

        for (const std::string& segment : CANONICAL_SEGMENTS) {

            if (!assignedSegments.contains(segment)) {
                if (!missingSegments.empty()) {
                    missingSegments += ", ";
                }

                missingSegments += segment;
            }
        }

        throw std::runtime_error("Sensor mapping is incomplete. Missing: " + missingSegments);
    }

    return SensorMapping(std::move(sensorToSegment));
}

const std::vector<std::string>& SensorMapping::canonicalSegments() {
    return CANONICAL_SEGMENTS;
}

const std::map<std::string, std::string>& SensorMapping::sensorToSegment() const {
    return sensorToSegment_;
}

std::optional<std::string> SensorMapping::sensorForSegment(const std::string& segment) const {

    const auto iterator = segmentToSensor_.find(segment);

    if (iterator == segmentToSensor_.end()) {
        return std::nullopt;
    }

    return iterator->second;
}

std::optional<std::string> SensorMapping::segmentForSensor(const std::string& sensorId) const {

    const auto iterator = sensorToSegment_.find(sensorId);

    if (iterator == sensorToSegment_.end()) {
        return std::nullopt;
    }

    return iterator->second;
}

std::size_t SensorMapping::size() const {
    return sensorToSegment_.size();
}
