#pragma once

#include "functional_calibration_types.h"
#include "seated_calibration.h"

#include <Eigen>
#include <chrono>
#include <iosfwd>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include <xsensdeviceapi.h>

class CalibrationSession;
class PacketCollector;
class SensorMapping;
namespace frontend_protocol {
class EventWriter;
}

/**
 * Identifies the proximal and distal sensors participating in one joint calibration.
 * @struct FunctionalSensorPair.
 */
struct FunctionalSensorPair {
    std::string proximalSensor;
    std::string distalSensor;
};

/**
 * Stores stationary-gravity and dynamic packets captured for one functional movement.
 * @struct FunctionalCalibrationCapture.
 */
struct FunctionalCalibrationCapture {
    FunctionalSensorPair sensors;
    GravityDirectionEstimate proximalGravity;
    GravityDirectionEstimate distalGravity;
    std::map<std::string, std::vector<XsDataPacket>> gravityPackets;
    std::map<std::string, std::vector<XsDataPacket>> dynamicPackets;
};

/**
 * Resolves and validates all stored prerequisites for a joint's sensor pair.
 * @param joint Functional joint definition containing proximal and distal segment names.
 * @param sensorToSegment Mapping from physical sensor ID to canonical segment name.
 * @param staticOffsets Committed static sensor-to-body rotations keyed by sensor ID.
 * @param finalOffsets Current final sensor-to-body rotations keyed by sensor ID.
 * @param gyroBiases Stationary sensor-frame gyroscope biases keyed by sensor ID.
 * @return Resolved proximal and distal IDs, or no value if any prerequisite is missing.
 */
std::optional<FunctionalSensorPair>
resolveFunctionalSensorPair(const calibration::JointCalibrationDefinition& joint,
                            const std::map<std::string, std::string>& sensorToSegment,
                            const std::map<std::string, Eigen::Quaterniond>& staticOffsets,
                            const std::map<std::string, Eigen::Quaterniond>& finalOffsets,
                            const std::map<std::string, Eigen::Vector3d>& gyroBiases);

/**
 * Performs timed gravity and functional-movement capture for one sensor pair.
 * @class FunctionalCalibrationCaptureWorkflow.
 */
class FunctionalCalibrationCaptureWorkflow {
  public:
    /**
     * Creates a functional capture workflow with configurable timing.
     * @param frontend Writer for guidance, state, result, and error events.
     * @param output Stream used for clinician instructions and progress.
     * @param errorOutput Stream used for rejected capture diagnostics.
     * @param preparationSeconds Countdown before each capture phase.
     * @param gravityCaptureSeconds Duration of the stationary gravity capture.
     * @param movementCaptureSeconds Duration of the repeated functional movement capture.
     */
    FunctionalCalibrationCaptureWorkflow(
        frontend_protocol::EventWriter& frontend, std::ostream& output, std::ostream& errorOutput,
        std::chrono::seconds preparationSeconds = std::chrono::seconds{5},
        std::chrono::seconds gravityCaptureSeconds = std::chrono::seconds{5},
        std::chrono::seconds movementCaptureSeconds = std::chrono::seconds{10});

    /**
     * Captures stationary gravity followed by the selected dynamic movement.
     * @param joint Selected functional joint definition.
     * @param sensorMapping Complete sensor-to-segment mapping.
     * @param calibrationSession Session containing required static offsets and gyro biases.
     * @param packetCollector Collector used to buffer both timed capture phases.
     * @return Captured packets, gravity vectors, and sensor IDs, or no value when rejected.
     */
    std::optional<FunctionalCalibrationCapture>
    capture(const calibration::JointCalibrationDefinition& joint,
            const SensorMapping& sensorMapping, const CalibrationSession& calibrationSession,
            PacketCollector& packetCollector) const;

  private:
    frontend_protocol::EventWriter& frontend_;
    std::ostream& output_;
    std::ostream& errorOutput_;
    std::chrono::seconds preparationSeconds_;
    std::chrono::seconds gravityCaptureSeconds_;
    std::chrono::seconds movementCaptureSeconds_;
};
