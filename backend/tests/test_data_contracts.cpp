#include "csv_schemas.h"
#include "output_naming.h"
#include "realtime_orientation_packet.h"
#include "segment_model_map.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <exception>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

void expect(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::size_t csvColumnCount(const std::string_view header) {
    return 1 + static_cast<std::size_t>(std::count(header.begin(), header.end(), ','));
}

void testRealtimePacketLayout() {
    using namespace SeatedMoCap::Realtime;

    expect(kProtocolVersion == 1, "Unexpected UDP protocol version.");
    expect(kSensorCount == SeatedMoCap::kSegmentModelMappings.size(),
           "UDP and OpenSim sensor counts differ.");
    expect(kDefaultPort == 9001, "Unexpected default UDP port.");
    expect(kAllSensorsValidMask == 0x1FFFFu, "Unexpected valid-sensor mask.");
    expect(kExpectedPacketSize == 300, "Unexpected UDP packet size.");
    expect(sizeof(OrientationFrameV1) == 300, "Packed UDP frame changed size.");

    expect(offsetof(OrientationFrameV1, magic) == 0, "magic offset changed.");
    expect(offsetof(OrientationFrameV1, version) == 4, "version offset changed.");
    expect(offsetof(OrientationFrameV1, sensorCount) == 6, "sensorCount offset changed.");
    expect(offsetof(OrientationFrameV1, sequence) == 8, "sequence offset changed.");
    expect(offsetof(OrientationFrameV1, timestampMicroseconds) == 16, "timestamp offset changed.");
    expect(offsetof(OrientationFrameV1, validSensorMask) == 24, "validSensorMask offset changed.");
    expect(offsetof(OrientationFrameV1, orientations) == 28, "orientation-array offset changed.");
}

void testSegmentOrderMatchesOpenSimMapping() {
    using namespace SeatedMoCap;

    const std::array<std::string_view, 17> expectedOrder{
        "Pelvis",        "Sternum",        "Head",           "Right_Shoulder", "Right_Upperarm",
        "Right_Forearm", "Right_Hand",     "Left_Shoulder",  "Left_Upperarm",  "Left_Forearm",
        "Left_Hand",     "Right_Upperleg", "Right_Lowerleg", "Right_Foot",     "Left_Upperleg",
        "Left_Lowerleg", "Left_Foot"};

    expect(Realtime::kSegmentOrder.size() == kSegmentModelMappings.size(),
           "UDP and OpenSim mapping counts differ.");
    expect(Realtime::kSegmentOrder == expectedOrder, "UDP segment order changed.");

    std::set<std::string_view> uniqueSegments;
    for (std::size_t index = 0; index < Realtime::kSegmentOrder.size(); ++index) {
        const std::string_view protocolSegment = Realtime::kSegmentOrder[index];
        const std::string_view modelSegment = kSegmentModelMappings[index].backendSegment;

        expect(protocolSegment == modelSegment, "UDP/OpenSim segment order differs.");
        expect(uniqueSegments.insert(protocolSegment).second, "Duplicate UDP segment.");
    }
}

void testCsvSchemas() {
    using namespace SeatedMoCap::CsvSchema;

    constexpr std::string_view expectedMeasurementHeader =
        "PacketID,TimeMilliseconds,SensorID,"
        "Q_Raw_GS_w,Q_Raw_GS_x,Q_Raw_GS_y,Q_Raw_GS_z,"
        "Q_Static_CB_w,Q_Static_CB_x,Q_Static_CB_y,Q_Static_CB_z,"
        "Q_Final_CB_w,Q_Final_CB_x,Q_Final_CB_y,Q_Final_CB_z,"
        "Gyro_S_X_rad_s,Gyro_S_Y_rad_s,Gyro_S_Z_rad_s,"
        "Acceleration_S_X_m_s2,Acceleration_S_Y_m_s2,Acceleration_S_Z_m_s2,"
        "MagneticField_S_X_au,MagneticField_S_Y_au,MagneticField_S_Z_au";
    constexpr std::string_view expectedJointAnglePrefix =
        "Sequence,TimestampMicroseconds,StreamTimeSeconds";

    expect(kMeasurementHeader == expectedMeasurementHeader, "Measurement CSV header changed.");
    expect(kOpenSimJointAnglePrefix == expectedJointAnglePrefix,
           "OpenSim joint-angle CSV prefix changed.");
    expect(csvColumnCount(kMeasurementHeader) == 24, "Measurement schema changed.");
    expect(kMeasurementHeader.starts_with("PacketID,TimeMilliseconds,SensorID,"),
           "Measurement schema prefix changed.");
    expect(kMeasurementHeader.ends_with("MagneticField_S_X_au,"
                                        "MagneticField_S_Y_au,"
                                        "MagneticField_S_Z_au"),
           "Measurement magnetic-field schema changed.");
    expect(csvColumnCount(kOpenSimJointAnglePrefix) == 3, "OpenSim joint-angle prefix changed.");
    expect(kOpenSimJointAnglePrefix.ends_with("StreamTimeSeconds"), "OpenSim time units changed.");
}

void testSessionNameSanitizationExamples() {
    const std::vector<std::pair<std::string, std::string>> examples{
        {"Patient 01 / chair", "Patient_01_chair"},
        {" Patient 08 / chair ", "Patient_08_chair"},
        {"P01--trial_2", "P01--trial_2"},
        {"///", "Session"},
        {"", "Session"},
    };

    for (const auto& [enteredName, expectedName] : examples) {
        expect(output_naming::safeFilenamePart(enteredName) == expectedName,
               "Session-name sanitization contract changed.");
    }
}

} // namespace

int main() {
    try {
        testRealtimePacketLayout();
        testSegmentOrderMatchesOpenSimMapping();
        testCsvSchemas();
        testSessionNameSanitizationExamples();
        std::cout << "Data-contract tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Data-contract test failed: " << error.what() << '\n';
        return 1;
    }
}
