#include "measurement_csv_writer.h"

#include "csv_schemas.h"
#include "seated_calibration.h"

#include <array>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace Eigen;

namespace {

struct RelativeOrientationDefinition {
    const char* name;
    const char* parentSegment;
    const char* childSegment;
};

// An empty parent means the child is expressed directly in the session frame.
constexpr std::array<RelativeOrientationDefinition, SeatedMoCap::Realtime::kSensorCount>
    kRelativeOrientationDefinitions{
        {{"Session_to_Pelvis", "", "Pelvis"},
         {"Pelvis_to_Sternum", "Pelvis", "Sternum"},
         {"Sternum_to_Head", "Sternum", "Head"},
         {"Sternum_to_Right_ShoulderGirdle", "Sternum", "Right_Shoulder"},
         {"Sternum_to_Left_ShoulderGirdle", "Sternum", "Left_Shoulder"},
         {"Right_ShoulderGirdle_to_Upperarm", "Right_Shoulder", "Right_Upperarm"},
         {"Left_ShoulderGirdle_to_Upperarm", "Left_Shoulder", "Left_Upperarm"},
         {"Right_Upperarm_to_Forearm", "Right_Upperarm", "Right_Forearm"},
         {"Left_Upperarm_to_Forearm", "Left_Upperarm", "Left_Forearm"},
         {"Right_Forearm_to_Hand", "Right_Forearm", "Right_Hand"},
         {"Left_Forearm_to_Hand", "Left_Forearm", "Left_Hand"},
         {"Pelvis_to_Right_Upperleg", "Pelvis", "Right_Upperleg"},
         {"Pelvis_to_Left_Upperleg", "Pelvis", "Left_Upperleg"},
         {"Right_Upperleg_to_Lowerleg", "Right_Upperleg", "Right_Lowerleg"},
         {"Left_Upperleg_to_Lowerleg", "Left_Upperleg", "Left_Lowerleg"},
         {"Right_Lowerleg_to_Foot", "Right_Lowerleg", "Right_Foot"},
         {"Left_Lowerleg_to_Foot", "Left_Lowerleg", "Left_Foot"}}};

void writeRelativeOrientationHeader(std::ostream& output) {
    output << "PacketID,TimeMilliseconds";

    for (const auto& definition : kRelativeOrientationDefinitions) {

        output << ',' << definition.name << "_Q_w" << ',' << definition.name << "_Q_x" << ','
               << definition.name << "_Q_y" << ',' << definition.name << "_Q_z";
    }

    output << '\n';
}

bool finiteQuaternion(const Quaterniond& quaternion) {
    return quaternion.coeffs().allFinite() && quaternion.squaredNorm() > 1e-12;
}

} // namespace

MeasurementCsvWriter::MeasurementCsvWriter(const std::map<std::string, std::string>& sensorSegments)
    : sensorSegments_(sensorSegments),
      writerThread_(&MeasurementCsvWriter::backgroundWriterLoop, this) {}

MeasurementCsvWriter::~MeasurementCsvWriter() {
    running_ = false;
    queueCondition_.notify_all();

    if (writerThread_.joinable()) {
        writerThread_.join();
    }

    std::lock_guard<std::mutex> fileLock(fileMutex_);
    if (csvFile_.is_open()) {
        csvFile_.close();
    }

    if (relativeOrientationCsvFile_.is_open()) {
        relativeOrientationCsvFile_.close();
    }
}

void MeasurementCsvWriter::enqueue(const std::string& sensorId, const XsDataPacket& packet) {
    // Calibration uses separate bounded buffers; avoid unnecessary callback work.
    if (!recording_.load(std::memory_order_relaxed)) {
        return;
    }

    {
        std::lock_guard<std::mutex> queueLock(queueMutex_);
        if (packetQueue_.size() >= MAX_QUEUED_PACKETS) {
            droppedPackets_.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        packetQueue_.push({sensorId, packet});
    }
    queueCondition_.notify_one();
}

void MeasurementCsvWriter::setCalibrationOffsets(
    const std::map<std::string, Quaterniond>& offsets) {

    std::lock_guard<std::mutex> lock(calibrationMutex_);
    calibrationOffsets_ = offsets;
}

void MeasurementCsvWriter::setStaticCalibrationOffsets(
    const std::map<std::string, Quaterniond>& offsets) {

    std::lock_guard<std::mutex> lock(calibrationMutex_);
    staticCalibrationOffsets_ = offsets;
}

void MeasurementCsvWriter::setGlobalToSessionRotation(const Quaterniond& q_CG) {

    std::lock_guard<std::mutex> lock(calibrationMutex_);
    globalToSession_ = q_CG.normalized();
}

bool MeasurementCsvWriter::startRecording(const std::string& filename) {
    recording_ = false;
    droppedPackets_ = 0;

    // Discard packets queued before the operator started this trial.
    {
        std::lock_guard<std::mutex> queueLock(queueMutex_);
        std::queue<std::pair<std::string, XsDataPacket>> empty;
        packetQueue_.swap(empty);
    }

    std::lock_guard<std::mutex> fileLock(fileMutex_);
    if (csvFile_.is_open()) {
        csvFile_.flush();
        csvFile_.close();
    }

    if (relativeOrientationCsvFile_.is_open()) {
        relativeOrientationCsvFile_.flush();
        relativeOrientationCsvFile_.close();
    }

    pendingOrientationFrames_.clear();

    filename_ = filename;
    csvFile_.open(filename_, std::ios::out | std::ios::trunc);

    const std::filesystem::path measurementPath{filename_};

    relativeOrientationFilename_ =
        (measurementPath.parent_path() /
         (measurementPath.stem().string() + "_RelativeSegmentOrientations.csv"))
            .string();

    relativeOrientationCsvFile_.open(relativeOrientationFilename_, std::ios::out | std::ios::trunc);

    if (!csvFile_.is_open() || !relativeOrientationCsvFile_.is_open()) {
        std::cerr << "ERROR: Failed to open measurement output files:\n"
                  << "  Sensor CSV: " << filename_ << '\n'
                  << "  Relative orientation CSV: " << relativeOrientationFilename_ << std::endl;

        if (csvFile_.is_open()) {
            csvFile_.close();
        }

        if (relativeOrientationCsvFile_.is_open()) {
            relativeOrientationCsvFile_.close();
        }

        return false;
    }

    csvFile_ << SeatedMoCap::CsvSchema::kMeasurementHeader << '\n';

    writeRelativeOrientationHeader(relativeOrientationCsvFile_);

    recording_ = true;
    return true;
}

void MeasurementCsvWriter::stopRecording() {
    recording_ = false;

    std::lock_guard<std::mutex> fileLock(fileMutex_);
    if (csvFile_.is_open()) {
        csvFile_.flush();
        csvFile_.close();
    }

    if (relativeOrientationCsvFile_.is_open()) {
        relativeOrientationCsvFile_.flush();
        relativeOrientationCsvFile_.close();
    }

    pendingOrientationFrames_.clear();

    const std::size_t dropped = droppedPackets_.exchange(0);
    if (dropped > 0) {
        std::cerr << "WARNING: CSV writer dropped " << dropped
                  << " packets because disk output could not keep up with 60 Hz input."
                  << std::endl;
    }
}

void MeasurementCsvWriter::accumulateRelativeOrientationFrame(std::uint64_t packetId,
                                                              std::uint64_t timeMilliseconds,
                                                              const std::string& sensorId,
                                                              const Quaterniond& q_CB) {

    if (!relativeOrientationCsvFile_.is_open() || !finiteQuaternion(q_CB)) {

        return;
    }

    const auto hardwareIt = sensorSegments_.find(sensorId);

    if (hardwareIt == sensorSegments_.end()) {
        return;
    }

    const std::string& segment = hardwareIt->second;
    auto& frame = pendingOrientationFrames_[packetId];

    if (frame.orientationsBySegment.empty()) {
        frame.timeMilliseconds = timeMilliseconds;
    }

    frame.orientationsBySegment[segment] = q_CB.normalized();

    // Write only complete frames sharing one Xsens packet identifier.
    if (frame.orientationsBySegment.size() == sensorSegments_.size()) {

        writeRelativeOrientationFrame(packetId, frame);

        pendingOrientationFrames_.erase(packetId);
    }

    // Bound incomplete frames when a sensor drops packets.
    while (pendingOrientationFrames_.size() > 100) {
        pendingOrientationFrames_.erase(pendingOrientationFrames_.begin());
    }
}

void MeasurementCsvWriter::writeRelativeOrientationFrame(const std::uint64_t packetId,
                                                         const PendingOrientationFrame& frame) {
    relativeOrientationCsvFile_ << std::setprecision(17) << packetId << ','
                                << frame.timeMilliseconds;

    for (const auto& definition : kRelativeOrientationDefinitions) {

        const auto childIt = frame.orientationsBySegment.find(definition.childSegment);

        if (childIt == frame.orientationsBySegment.end()) {

            // Preserve the fixed schema if a required segment is absent.
            relativeOrientationCsvFile_ << ",,,,";
            continue;
        }

        Quaterniond relativeQuaternion;

        if (definition.parentSegment[0] == '\0') {
            // The root remains relative to the session frame.
            relativeQuaternion = childIt->second.normalized();
        } else {
            const auto parentIt = frame.orientationsBySegment.find(definition.parentSegment);

            if (parentIt == frame.orientationsBySegment.end()) {

                relativeOrientationCsvFile_ << ",,,,";
                continue;
            }

            // q_PC = inverse(q_CP) * q_CC maps child coordinates into the parent frame.
            relativeQuaternion = (parentIt->second.conjugate() * childIt->second).normalized();
        }

        if (!finiteQuaternion(relativeQuaternion)) {
            relativeOrientationCsvFile_ << ",,,,";
            continue;
        }

        // Choose a consistent quaternion sign for CSV inspection.
        if (relativeQuaternion.w() < 0.0) {
            relativeQuaternion.coeffs() *= -1.0;
        }

        relativeOrientationCsvFile_ << ',' << relativeQuaternion.w() << ','
                                    << relativeQuaternion.x() << ',' << relativeQuaternion.y()
                                    << ',' << relativeQuaternion.z();
    }

    relativeOrientationCsvFile_ << '\n';
}

void MeasurementCsvWriter::backgroundWriterLoop() {
    while (true) {
        std::pair<std::string, XsDataPacket> data;
        {
            std::unique_lock<std::mutex> queueLock(queueMutex_);
            queueCondition_.wait(queueLock, [this] { return !packetQueue_.empty() || !running_; });

            if (!running_ && packetQueue_.empty()) {
                break;
            }

            data = std::move(packetQueue_.front());
            packetQueue_.pop();
        }

        const std::string& sensorId = data.first;
        const XsDataPacket& packet = data.second;

        std::lock_guard<std::mutex> fileLock(fileMutex_);
        if (!csvFile_.is_open() || !recording_) {
            continue;
        }

        if (packet.containsOrientation()) {
            const XsQuaternion xsQuaternion = packet.orientationQuaternion(XDI_CoordSysNwu);
            const Quaterniond qRaw(xsQuaternion.w(), xsQuaternion.x(), xsQuaternion.y(),
                                   xsQuaternion.z());
            Quaterniond qStaticCB = qRaw;
            Quaterniond qFinalCB = qRaw;

            {
                std::lock_guard<std::mutex> calibrationLock(calibrationMutex_);
                const Quaterniond qCS = (globalToSession_ * qRaw).normalized();

                const auto staticIt = staticCalibrationOffsets_.find(sensorId);
                if (staticIt != staticCalibrationOffsets_.end()) {
                    qStaticCB = SeatedCalibration::applyCalibration(qCS, staticIt->second);
                }

                const auto finalIt = calibrationOffsets_.find(sensorId);
                if (finalIt != calibrationOffsets_.end()) {
                    qFinalCB = SeatedCalibration::applyCalibration(qCS, finalIt->second);
                }
            }

            if (finiteQuaternion(qFinalCB)) {
                qFinalCB.normalize();

                accumulateRelativeOrientationFrame(
                    static_cast<std::uint64_t>(packet.packetId()),
                    static_cast<std::uint64_t>(packet.timeOfArrival().msTime()), sensorId,
                    qFinalCB);
            }

            csvFile_ << packet.packetId() << ',' << packet.timeOfArrival().msTime() << ','
                     << sensorId << ',' << qRaw.w() << ',' << qRaw.x() << ',' << qRaw.y() << ','
                     << qRaw.z() << ',' << qStaticCB.w() << ',' << qStaticCB.x() << ','
                     << qStaticCB.y() << ',' << qStaticCB.z() << ',' << qFinalCB.w() << ','
                     << qFinalCB.x() << ',' << qFinalCB.y() << ',' << qFinalCB.z() << ',';
        } else {
            csvFile_ << packet.packetId() << ',' << packet.timeOfArrival().msTime() << ','
                     << sensorId << ",0,0,0,0"
                     << ",0,0,0,0"
                     << ",0,0,0,0,";
        }

        if (packet.containsCalibratedGyroscopeData()) {
            const XsVector gyro = packet.calibratedGyroscopeData();
            csvFile_ << gyro[0] << ',' << gyro[1] << ',' << gyro[2] << ',';
        } else {
            csvFile_ << "0,0,0,";
        }

        if (packet.containsCalibratedAcceleration()) {
            const XsVector acceleration = packet.calibratedAcceleration();
            csvFile_ << acceleration[0] << ',' << acceleration[1] << ',' << acceleration[2] << ',';
        } else {
            csvFile_ << "0,0,0,";
        }

        if (packet.containsCalibratedMagneticField()) {
            const XsVector magneticField = packet.calibratedMagneticField();

            csvFile_ << magneticField[0] << ',' << magneticField[1] << ',' << magneticField[2]
                     << '\n';
        } else {
            csvFile_ << "0,0,0\n";
        }
    }
}
