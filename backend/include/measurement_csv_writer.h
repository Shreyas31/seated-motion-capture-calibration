#pragma once

#include "realtime_orientation_packet.h"

#include <Eigen>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <xsensdeviceapi.h>

/**
 * Asynchronously writes live MTw packets to a measurement CSV.
 * @class MeasurementCsvWriter.
 */
class MeasurementCsvWriter {
  public:
    /**
     * Starts the background writer and stores the anatomical sensor mapping.
     * @param sensorSegments Mapping from physical sensor ID to canonical segment name.
     */
    explicit MeasurementCsvWriter(const std::map<std::string, std::string>& sensorSegments);

    /** Stops recording, drains pending work, and joins the background thread. */
    ~MeasurementCsvWriter();

    MeasurementCsvWriter(const MeasurementCsvWriter&) = delete;
    MeasurementCsvWriter& operator=(const MeasurementCsvWriter&) = delete;

    /**
     * Queues one live packet for asynchronous processing.
     * @param sensorId Physical ID of the sensor that produced the packet.
     * @param packet Xsens packet copied into the writer queue.
     */
    void enqueue(const std::string& sensorId, const XsDataPacket& packet);

    /**
     * Replaces the final sensor-to-body offsets used for exported calibrated orientations.
     * @param offsets Final normalized rotations q_BS keyed by sensor ID.
     */
    void setCalibrationOffsets(const std::map<std::string, Eigen::Quaterniond>& offsets);

    /**
     * Replaces the immutable static offsets retained for comparison in exports.
     * @param offsets Static normalized rotations q_BS keyed by sensor ID.
     */
    void setStaticCalibrationOffsets(const std::map<std::string, Eigen::Quaterniond>& offsets);

    /**
     * Sets the heading transformation used to convert NWU data into the session frame.
     * @param q_CG Rotation from earth-fixed NWU frame G into session frame C.
     */
    void setGlobalToSessionRotation(const Eigen::Quaterniond& q_CG);

    /**
     * Opens the measurement and relative-segment-orientation CSV files for one trial.
     * @param filename Path of the primary measurement CSV file.
     * @return True when both output files open and recording is activated.
     */
    bool startRecording(const std::string& filename);

    /** Stops accepting rows for the active trial and closes both CSV files. */
    void stopRecording();

  private:
    // Ten seconds of full 17-sensor traffic at 60 Hz. If disk output falls
    // further behind, bounded loss is safer than unbounded memory growth.
    static constexpr std::size_t MAX_QUEUED_PACKETS = 60 * SeatedMoCap::Realtime::kSensorCount * 10;

    /** Dequeues packets and writes enabled measurement outputs until destruction. */
    void backgroundWriterLoop();

    /**
     * Accumulates segment orientations that share one Xsens packet identifier.
     * @struct PendingOrientationFrame.
     */
    struct PendingOrientationFrame {
        std::uint64_t timeMilliseconds = 0;
        std::map<std::string, Eigen::Quaterniond> orientationsBySegment;
    };

    /**
     * Adds one calibrated segment orientation to its pending synchronized frame.
     * @param packetId Xsens packet counter shared by the frame.
     * @param timeMilliseconds Xsens arrival time in milliseconds.
     * @param sensorId Physical sensor ID used to resolve the segment.
     * @param q_CB Calibrated body-to-session orientation.
     */
    void accumulateRelativeOrientationFrame(std::uint64_t packetId, std::uint64_t timeMilliseconds,
                                            const std::string& sensorId,
                                            const Eigen::Quaterniond& q_CB);

    /**
     * Writes one horizontal row of parent-to-child relative quaternions for a
     * complete synchronized frame.
     * @param packetId Xsens packet counter written to each generated row.
     * @param frame Accumulated segment orientations and timestamp.
     */
    void writeRelativeOrientationFrame(std::uint64_t packetId,
                                       const PendingOrientationFrame& frame);

    std::mutex queueMutex_;
    std::mutex calibrationMutex_;
    std::mutex fileMutex_;
    std::condition_variable queueCondition_;
    std::queue<std::pair<std::string, XsDataPacket>> packetQueue_;

    std::ofstream csvFile_;
    std::string filename_;
    std::ofstream relativeOrientationCsvFile_;
    std::string relativeOrientationFilename_;

    std::map<std::uint64_t, PendingOrientationFrame> pendingOrientationFrames_;
    std::thread writerThread_;
    std::atomic<bool> running_{true};
    std::atomic<bool> recording_{false};
    std::atomic<std::size_t> droppedPackets_{0};

    std::map<std::string, std::string> sensorSegments_;
    std::map<std::string, Eigen::Quaterniond> calibrationOffsets_;
    std::map<std::string, Eigen::Quaterniond> staticCalibrationOffsets_;
    Eigen::Quaterniond globalToSession_ = Eigen::Quaterniond::Identity();
};
