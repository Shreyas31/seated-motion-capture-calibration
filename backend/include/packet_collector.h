#pragma once

#include "measurement_csv_writer.h"

#include <cstddef>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <xsensdeviceapi.h>

/**
 * Xsens callback responsible only for receiving and buffering packets.
 * @class PacketCollector.
 */
class PacketCollector : public XsCallback {
  public:
    /**
     * Creates a collector that forwards each packet to the measurement writer.
     * @param measurementWriter Asynchronous writer whose lifetime must exceed the collector.
     */
    explicit PacketCollector(MeasurementCsvWriter& measurementWriter);

    /**
     * Returns a thread-safe snapshot of the newest packet from each sensor.
     * @return Packets keyed by physical sensor ID.
     */
    std::map<std::string, XsDataPacket> latestPackets() const;

    /** Clears all bounded calibration sample buffers under lock. */
    void clearCalibrationBuffers();

    /**
     * Moves all currently buffered calibration packets out of the collector.
     * @return Chronological packet vectors keyed by physical sensor ID.
     */
    std::map<std::string, std::vector<XsDataPacket>> takeCalibrationPackets();

  protected:
    /**
     * Stores and forwards one packet delivered by the Xsens callback thread.
     * @param device Device that produced the packet.
     * @param packet Live packet supplied by XDA.
     */
    void onLiveDataAvailable(XsDevice* device, const XsDataPacket* packet) override;

  private:
    static constexpr std::size_t MAX_SAMPLES_PER_SENSOR = 2000;

    MeasurementCsvWriter& measurementWriter_;
    mutable std::mutex latestMutex_;
    std::mutex sampleMutex_;
    std::map<std::string, XsDataPacket> latestPackets_;
    std::map<std::string, std::deque<XsDataPacket>> sampleBuffers_;
};
