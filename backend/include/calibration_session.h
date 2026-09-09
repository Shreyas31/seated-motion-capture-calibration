#pragma once

#include "seated_calibration.h"
#include "seated_pose_targets.h"

#include <Eigen>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

/**
 * Identifies a supported static seated-calibration reference pose.
 * @enum PoseType.
 */
enum class PoseType { Chair, Bed };

/**
 * Identifies the highest calibration stage completed for the current session.
 * @enum CalibrationState.
 */
enum class CalibrationState { Uncalibrated, StaticReady, FunctionallyRefined };

/**
 * Owns calibration settings and committed results for one acquisition session.
 * @class CalibrationSession.
 */
class CalibrationSession {
  public:
    /** Creates an uncalibrated session using the chair pose with palms up. */
    CalibrationSession();

    /**
     * Selects the reference pose and rebuilds its segment-orientation targets.
     * @param pose Static reference-pose category.
     */
    void selectPose(PoseType pose);

    /**
     * Returns the selected static reference pose.
     * @return Active pose category.
     */
    PoseType activePose() const;

    /**
     * Returns target segment orientations for the selected pose.
     * @return Map from canonical segment name to body-to-session quaternion q_CB.
     */
    const std::map<std::string, Eigen::Quaterniond>& activePoseTargets() const;

    /**
     * Atomically installs a complete static calibration and clears old
     * functional evidence.
     *
     * @param offsets Sensor-to-body rotations q_BS keyed by sensor ID.
     * @param gyroBiases Stationary sensor-frame gyroscope biases.
     * @param gravityEstimates Block-based gravity estimates keyed by sensor ID.
     * @param gravityUncertaintyFloorRadians Session gravity uncertainty floor,
     *        calculated as the median uncertainty across accepted sensors.
     * @param globalToSession Rotation q_CG from earth-fixed NWU into session C.
     */
    void commitStaticCalibration(std::map<std::string, Eigen::Quaterniond> offsets,
                                 std::map<std::string, Eigen::Vector3d> gyroBiases,
                                 std::map<std::string, GravityDirectionEstimate> gravityEstimates,
                                 double gravityUncertaintyFloorRadians,
                                 const Eigen::Quaterniond& globalToSession);

    /** Clears all calibration results after a failed static-calibration attempt. */
    void invalidateStaticCalibration();

    /**
     * Commits a paired observation-based functional refinement for one joint.
     * @param observations Complete accepted observation history keyed by sensor ID.
     * @param proximalSensor Physical ID of the proximal sensor.
     * @param proximalOffset Refined proximal sensor-to-body rotation q_BS.
     * @param distalSensor Physical ID of the distal sensor.
     * @param distalOffset Refined distal sensor-to-body rotation q_BS.
     * @throws std::out_of_range if either sensor has no committed static offset.
     */
    void commitFunctionalCalibration(
        std::map<std::string, std::vector<CalibrationVectorObservation>> observations,
        const std::string& proximalSensor, const Eigen::Quaterniond& proximalOffset,
        const std::string& distalSensor, const Eigen::Quaterniond& distalOffset);

    /**
     * Commits an observation-based refinement for one sensor.
     * @param observations Complete accepted observation history keyed by sensor ID.
     * @param sensor Physical ID of the refined sensor.
     * @param offset Refined sensor-to-body rotation q_BS.
     * @throws std::out_of_range if the sensor has no committed static offset.
     */
    void commitSingleFunctionalCalibration(
        std::map<std::string, std::vector<CalibrationVectorObservation>> observations,
        const std::string& sensor, const Eigen::Quaterniond& offset);

    /**
     * Reports whether a valid static calibration is available.
     * @return True for StaticReady or FunctionallyRefined states.
     */
    bool hasStaticCalibration() const;

    /**
     * Returns the current calibration progress state.
     * @return Current calibration state.
     */
    CalibrationState state() const;

    /**
     * Returns the immutable offsets produced by static calibration.
     * @return Sensor-to-body rotations q_BS keyed by sensor ID.
     */
    const std::map<std::string, Eigen::Quaterniond>& staticOffsets() const;

    /**
     * Returns offsets currently used for measurement and visualization.
     * @return Static or functionally refined sensor-to-body rotations q_BS.
     */
    const std::map<std::string, Eigen::Quaterniond>& finalOffsets() const;

    /**
     * Returns stationary gyroscope biases measured during static calibration.
     * @return Sensor-frame biases in radians per second, keyed by sensor ID.
     */
    const std::map<std::string, Eigen::Vector3d>& gyroBiases() const;

    /**
     * Returns the block-based gravity estimates recorded during the original
     * static calibration.
     * @return Gravity estimates keyed by sensor ID.
     */
    const std::map<std::string, GravityDirectionEstimate>& staticGravityEstimates() const;

    /**
     * Returns the fixed session gravity uncertainty floor in radians.
     * @return Session-level gravity uncertainty floor in radians.
     */
    double gravityUncertaintyFloorRadians() const;
    /**
     * Returns accepted vector observations used for functional refinement.
     * @return Observation lists keyed by sensor ID.
     */
    const std::map<std::string, std::vector<CalibrationVectorObservation>>&
    calibrationObservations() const;

    /**
     * Returns the session heading transformation established during static calibration.
     * @return Normalized rotation q_CG from earth-fixed NWU frame G into session frame C.
     */
    const Eigen::Quaterniond& globalToSessionRotation() const;

    /**
     * Advances and returns the static-capture sequence number.
     * @return One-based capture number for the next static export.
     */
    std::size_t nextStaticCaptureNumber();

    /**
     * Advances and returns the functional-capture number for one movement label.
     * @param jointLabel Stable joint or movement label used as the counter key.
     * @return One-based capture number for the label's next functional export.
     */
    std::size_t nextFunctionalCaptureNumber(const std::string& jointLabel);

  private:
    PoseType activePose_ = PoseType::Chair;
    CalibrationState state_ = CalibrationState::Uncalibrated;

    std::map<std::string, Eigen::Quaterniond> activePoseTargets_;
    std::map<std::string, Eigen::Quaterniond> staticOffsets_;
    std::map<std::string, Eigen::Quaterniond> finalOffsets_;
    std::map<std::string, Eigen::Vector3d> gyroBiases_;
    std::map<std::string, GravityDirectionEstimate> staticGravityEstimates_;
    double gravityUncertaintyFloorRadians_ = 0.0;

    std::map<std::string, std::vector<CalibrationVectorObservation>> observations_;

    Eigen::Quaterniond globalToSession_ = Eigen::Quaterniond::Identity();

    std::size_t staticCaptureNumber_ = 0;
    std::map<std::string, std::size_t> functionalCaptureNumbers_;
};
