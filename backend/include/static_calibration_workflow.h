#pragma once

#include "calibration_session.h"

#include <chrono>
#include <iosfwd>
#include <string>

class CalibrationCsvExporter;
class MeasurementCsvWriter;
class PacketCollector;
class SensorMapping;
struct SessionContext;
namespace frontend_protocol {
class EventWriter;
}

/**
 * Describes the selected static pose and its output labels.
 * @struct StaticPoseSelection.
 */
struct StaticPoseSelection {
    PoseType pose;
    std::string description;
    std::string fileLabel;
};

/**
 * Builds a validated static-pose selection from numeric interface choices.
 * @param poseChoice Pose choice where 1 is chair and 2 is bed.
 * @return Pose selection with clinician description and filename label.
 * @throws std::invalid_argument if choice is outside its supported range.
 */
StaticPoseSelection buildStaticPoseSelection(int poseChoice);

/**
 * Captures, validates, commits, and exports one complete static calibration.
 * @class StaticCalibrationWorkflow.
 */
class StaticCalibrationWorkflow {
  public:
    /**
     * Creates a timed static-calibration workflow.
     * @param frontend Writer for structured calibration and guidance events.
     * @param output Stream used for clinician instructions and results.
     * @param errorOutput Stream used for rejected-sensor and calibration errors.
     * @param preparationSeconds Countdown duration before packet capture begins.
     * @param captureSeconds Duration for which stationary packets are collected.
     */
    StaticCalibrationWorkflow(frontend_protocol::EventWriter& frontend, std::ostream& output,
                              std::ostream& errorOutput,
                              std::chrono::seconds preparationSeconds = std::chrono::seconds{5},
                              std::chrono::seconds captureSeconds = std::chrono::seconds{5});

    /**
     * Performs static calibration for all mapped sensors as one atomic operation.
     * @param selection Selected pose, palm orientation, and output labels.
     * @param sessionContext Session name and output directory.
     * @param sensorMapping Complete sensor-to-segment mapping.
     * @param calibrationSession Session state updated on success or invalidated on failure.
     * @param packetCollector Collector used to buffer the timed static capture.
     * @param measurementWriter Writer updated with accepted frame and offset transformations.
     * @param calibrationExporter Exporter used to save calibration evidence.
     * @return True when every mapped sensor is accepted and calibration is committed.
     * @throws std::exception if required mapped data or output operations fail unexpectedly.
     */
    bool run(const StaticPoseSelection& selection, const SessionContext& sessionContext,
             const SensorMapping& sensorMapping, CalibrationSession& calibrationSession,
             PacketCollector& packetCollector, MeasurementCsvWriter& measurementWriter,
             const CalibrationCsvExporter& calibrationExporter) const;

  private:
    frontend_protocol::EventWriter& frontend_;
    std::ostream& output_;
    std::ostream& errorOutput_;
    std::chrono::seconds preparationSeconds_;
    std::chrono::seconds captureSeconds_;
};
