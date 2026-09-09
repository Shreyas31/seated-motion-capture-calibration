#pragma once

#include <Eigen>
#include <atomic>
#include <cstdint>
#include <map>
#include <string>
#include <thread>

class PacketCollector;

/**
 * Streams calibrated 17-segment orientations to the local OpenSim viewer over UDP.
 * @class UdpOrientationStreamer.
 */
class UdpOrientationStreamer {
  public:
    /**
     * Creates a streamer backed by the latest Xsens packets and segment assignments.
     * @param packetCollector Collector that supplies the latest packet for each sensor.
     * @param sensorSegments Mapping from physical sensor ID to canonical segment name.
     */
    UdpOrientationStreamer(const PacketCollector& packetCollector,
                           const std::map<std::string, std::string>& sensorSegments);

    /** Stops and joins the streaming thread before releasing resources. */
    ~UdpOrientationStreamer();

    UdpOrientationStreamer(const UdpOrientationStreamer&) = delete;

    UdpOrientationStreamer& operator=(const UdpOrientationStreamer&) = delete;

    /**
     * Starts a 60 Hz stream using completed calibration transformations.
     * @param finalOffsets Sensor-to-anatomical-segment calibration offset for every sensor.
     * @param globalToSession Quaternion rotating the Xsens global frame into the session frame.
     * @param destinationAddress IPv4 destination address, normally the local loopback address.
     * @param destinationPort UDP destination port used by the OpenSim viewer.
     * @return True when inputs are valid and the streaming thread is started; otherwise false.
     */
    bool start(const std::map<std::string, Eigen::Quaterniond>& finalOffsets,
               const Eigen::Quaterniond& globalToSession,
               const std::string& destinationAddress = "127.0.0.1",
               std::uint16_t destinationPort = 9001);

    /** Stops streaming and waits for the worker thread to finish. */
    void stop();

    /**
     * Reports whether the streaming worker is currently active.
     * @return True while the worker loop is running.
     */
    [[nodiscard]] bool isRunning() const;

  private:
    /** Sends complete calibrated frames until stop is requested or socket setup fails. */
    void streamingLoop();

    const PacketCollector& packetCollector_;

    std::map<std::string, std::string> sensorSegments_;

    std::map<std::string, Eigen::Quaterniond> finalOffsets_;

    Eigen::Quaterniond globalToSession_ = Eigen::Quaterniond::Identity();

    std::string destinationAddress_;
    std::uint16_t destinationPort_ = 9001;

    std::thread streamingThread_;
    std::atomic<bool> running_{false};
};
