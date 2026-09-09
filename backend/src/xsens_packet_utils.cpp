#include "xsens_packet_utils.h"

namespace xsens_packet {

Eigen::Quaterniond orientationNwu(const XsDataPacket& packet) {
    const XsQuaternion q = packet.orientationQuaternion(XDI_CoordSysNwu);
    return Eigen::Quaterniond(q.w(), q.x(), q.y(), q.z()).normalized();
}

Eigen::Vector3d gyroscope(const XsDataPacket& packet) {
    const XsVector value = packet.calibratedGyroscopeData();
    return Eigen::Vector3d(value[0], value[1], value[2]);
}

Eigen::Vector3d acceleration(const XsDataPacket& packet) {
    const XsVector value = packet.calibratedAcceleration();
    return Eigen::Vector3d(value[0], value[1], value[2]);
}

} // namespace xsens_packet
