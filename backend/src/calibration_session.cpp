#include "calibration_session.h"

#include "seated_pose_targets.h"

#include <cmath>
#include <stdexcept>
#include <utility>

CalibrationSession::CalibrationSession() {
    selectPose(PoseType::Chair);
}

void CalibrationSession::selectPose(const PoseType pose) {
    activePose_ = pose;

    switch (pose) {
    case PoseType::Chair:
        activePoseTargets_ = buildChairPoseTargets();
        break;

    case PoseType::Bed:
        activePoseTargets_ = buildBedPoseTargets();
        break;
    }
}

PoseType CalibrationSession::activePose() const {
    return activePose_;
}

const std::map<std::string, Eigen::Quaterniond>& CalibrationSession::activePoseTargets() const {
    return activePoseTargets_;
}

void CalibrationSession::commitStaticCalibration(
    std::map<std::string, Eigen::Quaterniond> offsets,
    std::map<std::string, Eigen::Vector3d> gyroBiases,
    std::map<std::string, GravityDirectionEstimate> gravityEstimates,
    const double gravityUncertaintyFloorRadians, const Eigen::Quaterniond& globalToSession) {

    if (!std::isfinite(gravityUncertaintyFloorRadians) || gravityUncertaintyFloorRadians < 0.0) {

        throw std::invalid_argument("Static gravity uncertainty floor is invalid.");
    }

    if (offsets.size() != gyroBiases.size() || offsets.size() != gravityEstimates.size()) {

        throw std::invalid_argument("Static calibration maps have different sizes.");
    }

    // Validate all input maps before committing any session state.
    for (const auto& [sensorId, offset] : offsets) {
        const auto bias = gyroBiases.find(sensorId);

        const auto gravity = gravityEstimates.find(sensorId);

        if (bias == gyroBiases.end() || gravity == gravityEstimates.end()) {

            throw std::invalid_argument("Static calibration is missing data for sensor " +
                                        sensorId + ".");
        }

        if (!gravity->second.valid || !gravity->second.direction.allFinite() ||
            gravity->second.direction.norm() < 1e-9 ||
            !std::isfinite(gravity->second.angularUncertaintyRadians) ||
            gravity->second.angularUncertaintyRadians < 0.0) {

            throw std::invalid_argument("Static gravity estimate is invalid for sensor " +
                                        sensorId + ".");
        }
    }

    staticOffsets_ = std::move(offsets);

    finalOffsets_ = staticOffsets_;

    gyroBiases_ = std::move(gyroBiases);

    staticGravityEstimates_ = std::move(gravityEstimates);

    gravityUncertaintyFloorRadians_ = gravityUncertaintyFloorRadians;

    globalToSession_ = globalToSession.normalized();

    // Functional observations depend on the static sensor mounting.
    observations_.clear();

    state_ = CalibrationState::StaticReady;
}

void CalibrationSession::invalidateStaticCalibration() {
    staticOffsets_.clear();
    finalOffsets_.clear();
    gyroBiases_.clear();
    staticGravityEstimates_.clear();
    gravityUncertaintyFloorRadians_ = 0.0;
    observations_.clear();
    globalToSession_ = Eigen::Quaterniond::Identity();
    state_ = CalibrationState::Uncalibrated;
}

void CalibrationSession::commitFunctionalCalibration(
    std::map<std::string, std::vector<CalibrationVectorObservation>> observations,
    const std::string& proximalSensor, const Eigen::Quaterniond& proximalOffset,
    const std::string& distalSensor, const Eigen::Quaterniond& distalOffset) {

    // Commit the previously calculated pair atomically.
    observations_ = std::move(observations);
    finalOffsets_.at(proximalSensor) = proximalOffset.normalized();
    finalOffsets_.at(distalSensor) = distalOffset.normalized();
    state_ = CalibrationState::FunctionallyRefined;
}

void CalibrationSession::commitSingleFunctionalCalibration(
    std::map<std::string, std::vector<CalibrationVectorObservation>> observations,
    const std::string& sensor, const Eigen::Quaterniond& offset) {

    observations_ = std::move(observations);
    finalOffsets_.at(sensor) = offset.normalized();
    state_ = CalibrationState::FunctionallyRefined;
}

bool CalibrationSession::hasStaticCalibration() const {
    return state_ != CalibrationState::Uncalibrated;
}

CalibrationState CalibrationSession::state() const {
    return state_;
}

const std::map<std::string, Eigen::Quaterniond>& CalibrationSession::staticOffsets() const {
    return staticOffsets_;
}

const std::map<std::string, Eigen::Quaterniond>& CalibrationSession::finalOffsets() const {
    return finalOffsets_;
}

const std::map<std::string, Eigen::Vector3d>& CalibrationSession::gyroBiases() const {
    return gyroBiases_;
}

const std::map<std::string, GravityDirectionEstimate>&
CalibrationSession::staticGravityEstimates() const {

    return staticGravityEstimates_;
}

double CalibrationSession::gravityUncertaintyFloorRadians() const {

    return gravityUncertaintyFloorRadians_;
}

const std::map<std::string, std::vector<CalibrationVectorObservation>>&
CalibrationSession::calibrationObservations() const {
    return observations_;
}

const Eigen::Quaterniond& CalibrationSession::globalToSessionRotation() const {
    return globalToSession_;
}

std::size_t CalibrationSession::nextStaticCaptureNumber() {
    return ++staticCaptureNumber_;
}

std::size_t CalibrationSession::nextFunctionalCaptureNumber(const std::string& jointLabel) {
    return ++functionalCaptureNumbers_[jointLabel];
}
