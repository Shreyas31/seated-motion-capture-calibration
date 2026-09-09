#pragma once

#include <Eigen>
#include <xsensdeviceapi.h>

namespace xsens_packet {

/**
 * Reads and normalises an Xsens orientation in the North-West-Up convention.
 * @param packet Xsens packet containing orientation data.
 * @return Quaternion that rotates vectors from sensor frame S into global frame G.
 */
Eigen::Quaterniond orientationNwu(const XsDataPacket& packet);

/**
 * Reads calibrated angular velocity expressed in the sensor-fixed S frame.
 * @param packet Xsens packet containing calibrated gyroscope data.
 * @return Angular velocity vector in radians per second.
 */
Eigen::Vector3d gyroscope(const XsDataPacket& packet);

/**
 * Reads calibrated linear acceleration expressed in the sensor-fixed S frame.
 * @param packet Xsens packet containing calibrated acceleration data.
 * @return Acceleration vector in metres per second squared.
 */
Eigen::Vector3d acceleration(const XsDataPacket& packet);

} // namespace xsens_packet
