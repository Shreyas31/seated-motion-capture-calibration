#include "calibration_sample_analysis.h"

#include "seated_calibration.h"
#include "sensor_mapping.h"
#include "xsens_packet_utils.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>

using xsens_packet::acceleration;
using xsens_packet::gyroscope;
using xsens_packet::orientationNwu;

namespace calibration_analysis {

double quaternionAngularDistanceDegrees(const Eigen::Quaterniond& first,
                                        const Eigen::Quaterniond& second) {

    const Eigen::Quaterniond difference =
        (first.normalized().conjugate() * second.normalized()).normalized();
    const double scalar = std::clamp(std::abs(difference.w()), 0.0, 1.0);

    return 2.0 * std::acos(scalar) * 180.0 / std::acos(-1.0);
}

std::vector<Eigen::Vector3d> extractAccelerations(const std::vector<XsDataPacket>& packets) {

    std::vector<Eigen::Vector3d> samples;

    for (const XsDataPacket& packet : packets) {
        if (packet.containsCalibratedAcceleration()) {
            samples.push_back(acceleration(packet));
        }
    }

    return samples;
}

std::optional<Eigen::Vector3d>
estimateStationaryGyroBias(const std::vector<XsDataPacket>& packets) {

    Eigen::Vector3d sum = Eigen::Vector3d::Zero();
    std::size_t count = 0;

    for (const XsDataPacket& packet : packets) {
        if (!packet.containsCalibratedGyroscopeData()) {
            continue;
        }

        const Eigen::Vector3d gyro = gyroscope(packet);

        if (gyro.allFinite() && gyro.norm() < 0.25) {
            sum += gyro;
            ++count;
        }
    }

    if (count < 20) {
        return std::nullopt;
    }

    return sum / static_cast<double>(count);
}

std::vector<Eigen::Quaterniond>
extractStationaryOrientations(const std::vector<XsDataPacket>& packets) {

    std::vector<Eigen::Quaterniond> orientations;

    for (const XsDataPacket& packet : packets) {
        if (!packet.containsOrientation()) {
            continue;
        }

        if (packet.containsCalibratedGyroscopeData() && gyroscope(packet).norm() >= 0.25) {
            continue;
        }

        if (packet.containsCalibratedAcceleration()) {
            const double accelerationMagnitude = acceleration(packet).norm();

            if (accelerationMagnitude < 7.0 || accelerationMagnitude > 12.5) {
                continue;
            }
        }

        orientations.push_back(orientationNwu(packet));
    }

    return orientations;
}

std::optional<Eigen::Quaterniond>
averageStationaryOrientation(const std::map<std::string, std::vector<XsDataPacket>>& staticPackets,
                             const std::string& sensorId) {

    const auto packet = staticPackets.find(sensorId);

    if (packet == staticPackets.end()) {
        return std::nullopt;
    }

    return SeatedCalibration::averageQuaternions(extractStationaryOrientations(packet->second));
}

std::optional<Eigen::Quaterniond> estimateCalibrationRelativeFrame(
    const std::map<std::string, std::vector<XsDataPacket>>& staticPackets,
    const SensorMapping& sensorMapping) {

    const auto headSensor = sensorMapping.sensorForSegment("Head");

    if (!headSensor) {
        std::cerr << "Cannot define the session frame: head sensor is not mapped." << std::endl;
        return std::nullopt;
    }

    const auto headOrientation = averageStationaryOrientation(staticPackets, *headSensor);

    if (!headOrientation) {
        std::cerr << "Cannot define the session frame: no valid stationary head "
                  << "orientation was available." << std::endl;
        return std::nullopt;
    }

    return SeatedCalibration::computeGlobalToSessionYaw({*headOrientation},
                                                        Eigen::Vector3d::UnitX());
}

RelativeAxisSamples buildRelativeAxisSamples(const std::vector<XsDataPacket>& proximalPackets,
                                             const std::vector<XsDataPacket>& distalPackets,
                                             const Eigen::Vector3d& proximalBias,
                                             const Eigen::Vector3d& distalBias) {

    std::map<std::int64_t, const XsDataPacket*> distalByPacketId;

    for (const XsDataPacket& packet : distalPackets) {
        distalByPacketId[static_cast<std::int64_t>(packet.packetId())] = &packet;
    }

    RelativeAxisSamples result;

    for (const XsDataPacket& proximalPacket : proximalPackets) {
        const auto distal =
            distalByPacketId.find(static_cast<std::int64_t>(proximalPacket.packetId()));

        if (distal == distalByPacketId.end()) {
            continue;
        }

        const XsDataPacket& distalPacket = *distal->second;

        if (!proximalPacket.containsOrientation() || !distalPacket.containsOrientation() ||
            !proximalPacket.containsCalibratedGyroscopeData() ||
            !distalPacket.containsCalibratedGyroscopeData()) {
            continue;
        }

        const Eigen::Quaterniond q_GS_proximal = orientationNwu(proximalPacket);
        const Eigen::Quaterniond q_GS_distal = orientationNwu(distalPacket);
        const Eigen::Vector3d omegaProximalSensor = gyroscope(proximalPacket) - proximalBias;
        const Eigen::Vector3d omegaDistalSensor = gyroscope(distalPacket) - distalBias;
        const Eigen::Vector3d relativeOmegaGlobal =
            q_GS_distal * omegaDistalSensor - q_GS_proximal * omegaProximalSensor;

        if (!relativeOmegaGlobal.allFinite() || relativeOmegaGlobal.norm() < 0.35) {
            continue;
        }

        result.proximal.push_back(q_GS_proximal.conjugate() * relativeOmegaGlobal);
        result.distal.push_back(q_GS_distal.conjugate() * relativeOmegaGlobal);
        ++result.synchronizedPackets;
    }

    return result;
}

ReturnPoseValidationResult validateReturnToStaticPose(const std::vector<XsDataPacket>& packets,
                                                      const Eigen::Quaterniond& q_CB_target,
                                                      const Eigen::Quaterniond& q_BS_candidate,
                                                      const Eigen::Quaterniond& q_BS_static,
                                                      const Eigen::Quaterniond& q_CG) {

    ReturnPoseValidationResult result;
    constexpr std::size_t MINIMUM_STATIONARY_SAMPLES = 20;
    const std::vector<Eigen::Quaterniond> orientations_GS = extractStationaryOrientations(packets);
    result.stationarySamples = orientations_GS.size();

    if (orientations_GS.size() < MINIMUM_STATIONARY_SAMPLES) {
        result.message = "Too few stationary return-pose samples.";
        return result;
    }

    const auto average_q_GS = SeatedCalibration::averageQuaternions(orientations_GS);

    if (!average_q_GS) {
        result.message = "Could not average return-pose orientations.";
        return result;
    }

    const Eigen::Quaterniond q_CS = (q_CG.normalized() * average_q_GS->normalized()).normalized();
    const Eigen::Quaterniond q_CB_candidate =
        SeatedCalibration::applyCalibration(q_CS, q_BS_candidate);
    result.candidateErrorDegrees = quaternionAngularDistanceDegrees(q_CB_target, q_CB_candidate);
    const Eigen::Quaterniond q_CB_static = SeatedCalibration::applyCalibration(q_CS, q_BS_static);
    result.staticReferenceErrorDegrees = quaternionAngularDistanceDegrees(q_CB_target, q_CB_static);
    result.valid = true;
    result.message = "Return-to-static-pose orientation calculated.";
    return result;
}

} // namespace calibration_analysis
