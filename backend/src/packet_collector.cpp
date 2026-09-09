#include "packet_collector.h"

#include <utility>

PacketCollector::PacketCollector(MeasurementCsvWriter& measurementWriter)
    : measurementWriter_(measurementWriter) {}

void PacketCollector::onLiveDataAvailable(XsDevice* device, const XsDataPacket* packet) {

    const std::string sensorId = device->deviceId().toString().toStdString();

    {
        std::lock_guard<std::mutex> latestLock(latestMutex_);
        latestPackets_[sensorId] = *packet;
    }

    {
        std::lock_guard<std::mutex> sampleLock(sampleMutex_);
        auto& buffer = sampleBuffers_[sensorId];
        buffer.push_back(*packet);
        if (buffer.size() > MAX_SAMPLES_PER_SENSOR) {
            buffer.pop_front();
        }
    }

    measurementWriter_.enqueue(sensorId, *packet);
}

std::map<std::string, XsDataPacket> PacketCollector::latestPackets() const {
    std::lock_guard<std::mutex> latestLock(latestMutex_);
    return latestPackets_;
}

void PacketCollector::clearCalibrationBuffers() {
    std::lock_guard<std::mutex> sampleLock(sampleMutex_);
    sampleBuffers_.clear();
}

std::map<std::string, std::vector<XsDataPacket>> PacketCollector::takeCalibrationPackets() {
    std::lock_guard<std::mutex> sampleLock(sampleMutex_);

    std::map<std::string, std::vector<XsDataPacket>> result;
    for (auto& [sensorId, packets] : sampleBuffers_) {
        result.emplace(sensorId, std::vector<XsDataPacket>(packets.begin(), packets.end()));
    }
    sampleBuffers_.clear();
    return result;
}
