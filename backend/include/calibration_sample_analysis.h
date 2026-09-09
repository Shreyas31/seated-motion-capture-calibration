#pragma once

#include <Eigen>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include <xsensdeviceapi.h>

class SensorMapping;

namespace calibration_analysis {

/**
 * Summarises whether a sensor returned sufficiently close to its static reference pose.
 * @struct ReturnPoseValidationResult.
 */
struct ReturnPoseValidationResult {
    bool valid = false;
    double candidateErrorDegrees = 0.0;
    double staticReferenceErrorDegrees = 0.0;
    std::size_t stationarySamples = 0;
    std::string message;
};

/**
 * Stores synchronized proximal and distal angular-velocity samples for axis estimation.
 * @struct RelativeAxisSamples.
 */
struct RelativeAxisSamples {
    std::vector<Eigen::Vector3d> proximal;
    std::vector<Eigen::Vector3d> distal;
    std::size_t synchronizedPackets = 0;
};

/**
 * Computes the shortest rotational distance between two orientations.
 * @param first First quaternion.
 * @param second Second quaternion.
 * @return Absolute angular separation in degrees in the range [0, 180].
 */
double quaternionAngularDistanceDegrees(const Eigen::Quaterniond& first,
                                        const Eigen::Quaterniond& second);

/**
 * Extracts calibrated acceleration samples from packets that contain them.
 * @param packets Chronological Xsens packets from one sensor.
 * @return Acceleration vectors in the sensor frame, in metres per second squared.
 */
std::vector<Eigen::Vector3d> extractAccelerations(const std::vector<XsDataPacket>& packets);

/**
 * Estimates stationary gyroscope bias from valid calibrated samples.
 * @param packets Static Xsens packets from one sensor.
 * @return Mean sensor-frame angular velocity in radians per second, or no value if unavailable.
 */
std::optional<Eigen::Vector3d> estimateStationaryGyroBias(const std::vector<XsDataPacket>& packets);

/**
 * Extracts normalized NWU orientations from stationary packets.
 * @param packets Static Xsens packets from one sensor.
 * @return Chronological sensor-to-global quaternions q_GS.
 */
std::vector<Eigen::Quaterniond>
extractStationaryOrientations(const std::vector<XsDataPacket>& packets);

/**
 * Computes the mean stationary orientation for one sensor.
 * @param staticPackets Static packet buffers keyed by sensor ID.
 * @param sensorId Sensor whose orientation is requested.
 * @return Mean sensor-to-global quaternion q_GS, or no value if samples are unavailable.
 */
std::optional<Eigen::Quaterniond>
averageStationaryOrientation(const std::map<std::string, std::vector<XsDataPacket>>& staticPackets,
                             const std::string& sensorId);

/**
 * Estimates the session heading from the configured torso and pelvis sensors.
 * @param staticPackets Static packet buffers keyed by sensor ID.
 * @param sensorMapping Validated mapping between sensor IDs and anatomical segments.
 * @return Global-to-session yaw rotation q_CG, or no value when it cannot be estimated.
 */
std::optional<Eigen::Quaterniond> estimateCalibrationRelativeFrame(
    const std::map<std::string, std::vector<XsDataPacket>>& staticPackets,
    const SensorMapping& sensorMapping);

/**
 * Builds synchronized bias-corrected angular-velocity samples for a paired joint.
 * @param proximalPackets Chronological packets from the proximal sensor.
 * @param distalPackets Chronological packets from the distal sensor.
 * @param proximalBias Proximal stationary gyroscope bias in radians per second.
 * @param distalBias Distal stationary gyroscope bias in radians per second.
 * @return Paired sensor-frame angular-velocity samples and synchronization count.
 */
RelativeAxisSamples buildRelativeAxisSamples(const std::vector<XsDataPacket>& proximalPackets,
                                             const std::vector<XsDataPacket>& distalPackets,
                                             const Eigen::Vector3d& proximalBias,
                                             const Eigen::Vector3d& distalBias);

/**
 * Compares a return-pose candidate and the original static offset against a target pose.
 * @param packets Return-pose packets from one sensor.
 * @param q_CB_target Target body-to-session orientation for the segment.
 * @param q_BS_candidate Candidate sensor-to-body calibration offset.
 * @param q_BS_static Original static sensor-to-body calibration offset.
 * @param q_CG Rotation from earth-fixed NWU frame G into session frame C.
 * @return Validation decision, angular errors in degrees, sample count, and diagnostic message.
 */
ReturnPoseValidationResult validateReturnToStaticPose(const std::vector<XsDataPacket>& packets,
                                                      const Eigen::Quaterniond& q_CB_target,
                                                      const Eigen::Quaterniond& q_BS_candidate,
                                                      const Eigen::Quaterniond& q_BS_static,
                                                      const Eigen::Quaterniond& q_CG);

} // namespace calibration_analysis
