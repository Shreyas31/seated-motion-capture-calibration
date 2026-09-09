#include "calibration_csv_exporter.h"

#include "seated_calibration.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <ostream>

using namespace Eigen;

namespace {

std::string csvText(const std::string& value) {
    std::string escaped = value;
    std::size_t position = 0;
    while ((position = escaped.find('"', position)) != std::string::npos) {
        escaped.insert(position, 1, '"');
        position += 2;
    }
    return "\"" + escaped + "\"";
}

void writeQuaternion(std::ostream& output, const Quaterniond& quaternion) {

    const Quaterniond normalized = quaternion.normalized();
    output << normalized.w() << ',' << normalized.x() << ',' << normalized.y() << ','
           << normalized.z();
}

} // namespace

bool CalibrationCsvExporter::saveCapture(
    const std::string& filename, const std::string& sessionName, const std::string& calibrationType,
    const std::string& capturePhase, const std::string& jointName, const std::string& poseName,
    const std::map<std::string, std::vector<XsDataPacket>>& packets,
    const std::map<std::string, std::string>& sensorSegments,
    const std::map<std::string, Quaterniond>& staticOffsets,
    const std::map<std::string, Quaterniond>& finalOffsets,
    const std::map<std::string, Quaterniond>& poseTargets, const Quaterniond& q_CG_input,
    const bool append) const {

    const bool writeHeader = !append || !std::ifstream(filename, std::ios::binary).good();
    std::ofstream output(filename, append ? (std::ios::out | std::ios::app) : std::ios::out);
    if (!output.is_open()) {
        std::cerr << "ERROR: Failed to open calibration CSV: " << filename << std::endl;
        return false;
    }

    output << std::setprecision(15);
    if (writeHeader) {
        output << "Session,CalibrationType,CapturePhase,Joint,Pose,"
               << "PacketID,Time,SensorID,Segment,"
               << "Q_Raw_GS_w,Q_Raw_GS_x,Q_Raw_GS_y,Q_Raw_GS_z,"
               << "Q_Static_CB_w,Q_Static_CB_x,Q_Static_CB_y,Q_Static_CB_z,"
               << "Q_Final_CB_w,Q_Final_CB_x,Q_Final_CB_y,Q_Final_CB_z,"
               << "Q_Target_CB_w,Q_Target_CB_x,Q_Target_CB_y,Q_Target_CB_z,"
               << "Q_Static_BS_w,Q_Static_BS_x,Q_Static_BS_y,Q_Static_BS_z,"
               << "Q_Final_BS_w,Q_Final_BS_x,Q_Final_BS_y,Q_Final_BS_z,"
               << "Q_CG_w,Q_CG_x,Q_CG_y,Q_CG_z,"
               << "Gyro_X,Gyro_Y,Gyro_Z,GyroNorm,"
               << "Acc_X,Acc_Y,Acc_Z,AccNorm\n";
    }

    const Quaterniond q_CG = q_CG_input.normalized();
    for (const auto& [sensorId, sensorPackets] : packets) {
        const auto segmentIt = sensorSegments.find(sensorId);
        const std::string segment = (segmentIt == sensorSegments.end()) ? "" : segmentIt->second;
        const auto staticIt = staticOffsets.find(sensorId);
        const auto finalIt = finalOffsets.find(sensorId);
        const auto targetIt = poseTargets.find(segment);

        for (const XsDataPacket& packet : sensorPackets) {
            if (!packet.containsOrientation()) {
                continue;
            }

            const XsQuaternion xsQuaternion = packet.orientationQuaternion(XDI_CoordSysNwu);
            const Quaterniond q_GS(xsQuaternion.w(), xsQuaternion.x(), xsQuaternion.y(),
                                   xsQuaternion.z());
            const Quaterniond q_CS = (q_CG * q_GS).normalized();
            const Quaterniond qStaticCB =
                (staticIt == staticOffsets.end())
                    ? Quaterniond::Identity()
                    : SeatedCalibration::applyCalibration(q_CS, staticIt->second);
            const Quaterniond qFinalCB =
                (finalIt == finalOffsets.end())
                    ? Quaterniond::Identity()
                    : SeatedCalibration::applyCalibration(q_CS, finalIt->second);
            const Quaterniond qTargetCB = (targetIt == poseTargets.end())
                                              ? Quaterniond::Identity()
                                              : targetIt->second.normalized();
            const Quaterniond qStaticBS = (staticIt == staticOffsets.end())
                                              ? Quaterniond::Identity()
                                              : staticIt->second.normalized();
            const Quaterniond qFinalBS = (finalIt == finalOffsets.end())
                                             ? Quaterniond::Identity()
                                             : finalIt->second.normalized();

            Vector3d gyro = Vector3d::Zero();
            if (packet.containsCalibratedGyroscopeData()) {
                const XsVector value = packet.calibratedGyroscopeData();
                gyro = Vector3d(value[0], value[1], value[2]);
            }

            // Calibrated acceleration retains gravity, which is required to
            // reproduce the gravity observation used by calibration.
            Vector3d acceleration = Vector3d::Zero();
            if (packet.containsCalibratedAcceleration()) {
                const XsVector value = packet.calibratedAcceleration();
                acceleration = Vector3d(value[0], value[1], value[2]);
            }

            output << csvText(sessionName) << ',' << csvText(calibrationType) << ','
                   << csvText(capturePhase) << ',' << csvText(jointName) << ',' << csvText(poseName)
                   << ',' << packet.packetId() << ',' << packet.timeOfArrival().msTime() << ','
                   << csvText(sensorId) << ',' << csvText(segment) << ',';
            writeQuaternion(output, q_GS);
            output << ',';
            writeQuaternion(output, qStaticCB);
            output << ',';
            writeQuaternion(output, qFinalCB);
            output << ',';
            writeQuaternion(output, qTargetCB);
            output << ',';
            writeQuaternion(output, qStaticBS);
            output << ',';
            writeQuaternion(output, qFinalBS);
            output << ',';
            writeQuaternion(output, q_CG);
            output << ',' << gyro.x() << ',' << gyro.y() << ',' << gyro.z() << ',' << gyro.norm()
                   << ',' << acceleration.x() << ',' << acceleration.y() << ',' << acceleration.z()
                   << ',' << acceleration.norm() << '\n';
        }
    }

    return output.good();
}
