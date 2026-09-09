#pragma once

#include "functional_calibration_capture.h"
#include "functional_calibration_solver.h"
#include "functional_return_pose.h"

#include <iosfwd>
#include <optional>

class CalibrationCsvExporter;
class CalibrationSession;
class MeasurementCsvWriter;
class PacketCollector;
class SensorMapping;
struct SessionContext;
namespace frontend_protocol {
class EventWriter;
}

/**
 * Presents the supported functional movements and resolves one clinician selection.
 * @class FunctionalCalibrationSelectionWorkflow.
 */
class FunctionalCalibrationSelectionWorkflow {
  public:
    /**
     * Creates a movement-selection workflow using injected protocol and console streams.
     * @param frontend Writer for structured frontend events.
     * @param input Stream from which the numeric movement choice is read.
     * @param output Stream used for prompts and menu content.
     * @param errorOutput Stream used for invalid-choice diagnostics.
     */
    FunctionalCalibrationSelectionWorkflow(frontend_protocol::EventWriter& frontend,
                                           std::istream& input, std::ostream& output,
                                           std::ostream& errorOutput);

    /**
     * Reads one movement choice without internally retrying invalid input.
     * @return Selected joint definition, or no value when input is malformed or unsupported.
     */
    std::optional<calibration::JointCalibrationDefinition> collect() const;

  private:
    frontend_protocol::EventWriter& frontend_;
    std::istream& input_;
    std::ostream& output_;
    std::ostream& errorOutput_;
};

/**
 * Synchronizes accepted functional results with measurement output and evidence exports.
 * @class FunctionalCalibrationFinalizer.
 */
class FunctionalCalibrationFinalizer {
  public:
    /**
     * Creates a finalization stage using injected reporting channels.
     * @param frontend Writer for structured completion events.
     * @param output Stream used for saved-file and completion messages.
     */
    FunctionalCalibrationFinalizer(frontend_protocol::EventWriter& frontend, std::ostream& output);

    /**
     * Applies committed offsets to measurement output and exports all functional evidence.
     * @param joint Completed functional joint definition.
     * @param capture Original gravity and movement capture.
     * @param axisAnalysis Estimated axes and sample-quality results.
     * @param returnPose Return-pose packets, validations, and accepted refinements.
     * @param sessionContext Session name and output directory.
     * @param sensorMapping Complete sensor-to-segment mapping.
     * @param calibrationSession Session containing the newly committed final offsets.
     * @param measurementWriter Writer updated to use the committed offsets.
     * @param calibrationExporter Exporter used to save capture evidence.
     */
    void finalize(const calibration::JointCalibrationDefinition& joint,
                  const FunctionalCalibrationCapture& capture,
                  const FunctionalAxisAnalysis& axisAnalysis,
                  const FunctionalReturnPoseResult& returnPose,
                  const SessionContext& sessionContext, const SensorMapping& sensorMapping,
                  CalibrationSession& calibrationSession, MeasurementCsvWriter& measurementWriter,
                  const CalibrationCsvExporter& calibrationExporter) const;

  private:
    frontend_protocol::EventWriter& frontend_;
    std::ostream& output_;
};

/**
 * Coordinates one complete functional-calibration attempt.
 *
 * Each specialised stage retains its own acquisition or mathematical
 * responsibility. This class owns only sequencing and early-exit policy.
 * @class FunctionalCalibrationWorkflow.
 */
class FunctionalCalibrationWorkflow {
  public:
    /**
     * Creates the complete functional-calibration pipeline.
     * @param frontend Writer shared by all functional-calibration stages.
     * @param input Stream used for movement selection.
     * @param output Stream used for clinician guidance and results.
     * @param errorOutput Stream used for rejected-stage diagnostics.
     */
    FunctionalCalibrationWorkflow(frontend_protocol::EventWriter& frontend, std::istream& input,
                                  std::ostream& output, std::ostream& errorOutput);

    /**
     * Runs one selected functional-calibration attempt through commit and export.
     * @param sessionContext Session name and output directory.
     * @param sensorMapping Complete sensor-to-segment mapping.
     * @param calibrationSession Static calibration input and accepted-result destination.
     * @param packetCollector Collector used by all timed capture phases.
     * @param measurementWriter Writer updated after a successful commit.
     * @param calibrationExporter Exporter used to preserve calibration evidence.
     * @return True only when every stage succeeds and finalization completes.
     */
    bool run(const SessionContext& sessionContext, const SensorMapping& sensorMapping,
             CalibrationSession& calibrationSession, PacketCollector& packetCollector,
             MeasurementCsvWriter& measurementWriter,
             const CalibrationCsvExporter& calibrationExporter);

  private:
    frontend_protocol::EventWriter& frontend_;
    std::ostream& errorOutput_;
    FunctionalCalibrationSelectionWorkflow selection_;
    FunctionalCalibrationCaptureWorkflow capture_;
    FunctionalAxisWorkflow axes_;
    FunctionalRefinementWorkflow refinement_;
    FunctionalReturnPoseWorkflow returnPose_;
    FunctionalCalibrationFinalizer finalizer_;
};
