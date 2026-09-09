#pragma once

#include <Eigen>
#include <map>
#include <string>
#include <vector>
#include <xsensdeviceapi.h>

/**
 * Stateless exporter for already-buffered static and functional captures.
 * @class CalibrationCsvExporter.
 */
class CalibrationCsvExporter {
  public:
    /**
     * Writes buffered calibration packets and associated transformations to CSV.
     * @param filename Destination CSV filename.
     * @param sessionName Identifier of the subject session.
     * @param calibrationType Static or functional calibration label.
     * @param capturePhase Phase represented by the buffered packets.
     * @param jointName Functional joint label, or an empty string for static calibration.
     * @param poseName Selected static reference-pose label.
     * @param packets Buffered Xsens packets keyed by sensor ID.
     * @param sensorSegments Mapping from sensor ID to canonical segment name.
     * @param staticOffsets Static sensor-to-body rotations q_BS keyed by sensor ID.
     * @param finalOffsets Current final sensor-to-body rotations q_BS keyed by sensor ID.
     * @param poseTargets Target body-to-session rotations q_CB keyed by segment name.
     * @param q_CG Rotation from the earth-fixed NWU frame G into session frame C.
     * @param append True to append rows to an existing file; false to replace it.
     * @return True when all rows are written successfully; otherwise false.
     */
    bool saveCapture(const std::string& filename, const std::string& sessionName,
                     const std::string& calibrationType, const std::string& capturePhase,
                     const std::string& jointName, const std::string& poseName,
                     const std::map<std::string, std::vector<XsDataPacket>>& packets,
                     const std::map<std::string, std::string>& sensorSegments,
                     const std::map<std::string, Eigen::Quaterniond>& staticOffsets,
                     const std::map<std::string, Eigen::Quaterniond>& finalOffsets,
                     const std::map<std::string, Eigen::Quaterniond>& poseTargets,
                     const Eigen::Quaterniond& q_CG, bool append = false) const;
};
