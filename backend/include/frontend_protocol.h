#pragma once

#include <array>
#include <iosfwd>
#include <string>
#include <string_view>

namespace frontend_protocol {

/** Stable top-level event categories mirrored by frontend/protocol.py. */
namespace category {
inline constexpr std::string_view kState = "STATE";
inline constexpr std::string_view kResult = "RESULT";
inline constexpr std::string_view kError = "ERROR";
inline constexpr std::string_view kGuidance = "GUIDANCE";
inline constexpr std::array<std::string_view, 4> kAll{kState, kResult, kError, kGuidance};
} // namespace category

/** Stable workflow state names mirrored by frontend/protocol.py. */
namespace state {
inline constexpr std::string_view kSensors = "SENSORS";
inline constexpr std::string_view kHeading = "HEADING";
inline constexpr std::string_view kSession = "SESSION";
inline constexpr std::string_view kMenu = "MENU";
inline constexpr std::string_view kJoint = "JOINT";
inline constexpr std::string_view kRecording = "RECORDING";
inline constexpr std::string_view kExiting = "EXITING";
inline constexpr std::array<std::string_view, 7> kAll{kSensors, kHeading,   kSession, kMenu,
                                                      kJoint,   kRecording, kExiting};
} // namespace state

/** Stable successful-result names mirrored by frontend/protocol.py. */
namespace result {
inline constexpr std::string_view kSensorStatus = "SENSOR_STATUS";
inline constexpr std::string_view kUnmappedSensor = "UNMAPPED_SENSOR";
inline constexpr std::string_view kSensorsConnected = "SENSORS_CONNECTED";
inline constexpr std::string_view kSessionConfigured = "SESSION_CONFIGURED";
inline constexpr std::string_view kStaticCalibration = "STATIC_CALIBRATION";
inline constexpr std::string_view kFunctionalCalibration = "FUNCTIONAL_CALIBRATION";
inline constexpr std::string_view kRecordingStarted = "RECORDING_STARTED";
inline constexpr std::string_view kRecordingStopped = "RECORDING_STOPPED";
inline constexpr std::array<std::string_view, 8> kAll{
    kSensorStatus,      kUnmappedSensor,        kSensorsConnected, kSessionConfigured,
    kStaticCalibration, kFunctionalCalibration, kRecordingStarted, kRecordingStopped};
} // namespace result

/** Stable error names mirrored by frontend/protocol.py. */
namespace error {
inline constexpr std::string_view kSensorsIncomplete = "SENSORS_INCOMPLETE";
inline constexpr std::string_view kStaticCalibration = "STATIC_CALIBRATION";
inline constexpr std::string_view kFunctionalCalibration = "FUNCTIONAL_CALIBRATION";
inline constexpr std::string_view kRecording = "RECORDING";
inline constexpr std::array<std::string_view, 4> kAll{kSensorsIncomplete, kStaticCalibration,
                                                      kFunctionalCalibration, kRecording};
} // namespace error

/** Stable guidance phase names mirrored by frontend/protocol.py. */
namespace guidance {
inline constexpr std::string_view kStaticPreparation = "STATIC_PREPARATION";
inline constexpr std::string_view kStaticCapture = "STATIC_CAPTURE";
inline constexpr std::string_view kStaticProcessing = "STATIC_PROCESSING";
inline constexpr std::string_view kFunctionalGravityPreparation = "FUNCTIONAL_GRAVITY_PREPARATION";
inline constexpr std::string_view kFunctionalGravityCapture = "FUNCTIONAL_GRAVITY_CAPTURE";
inline constexpr std::string_view kFunctionalGravityProcessing = "FUNCTIONAL_GRAVITY_PROCESSING";
inline constexpr std::string_view kFunctionalMovementPreparation =
    "FUNCTIONAL_MOVEMENT_PREPARATION";
inline constexpr std::string_view kFunctionalMovementCapture = "FUNCTIONAL_MOVEMENT_CAPTURE";
inline constexpr std::string_view kFunctionalReturnPosePreparation =
    "FUNCTIONAL_RETURN_POSE_PREPARATION";
inline constexpr std::string_view kFunctionalReturnPoseCapture = "FUNCTIONAL_RETURN_POSE_CAPTURE";
inline constexpr std::string_view kFunctionalProcessing = "FUNCTIONAL_PROCESSING";
inline constexpr std::string_view kCountdown = "COUNTDOWN";
inline constexpr std::array<std::string_view, 12> kAll{kStaticPreparation,
                                                       kStaticCapture,
                                                       kStaticProcessing,
                                                       kFunctionalGravityPreparation,
                                                       kFunctionalGravityCapture,
                                                       kFunctionalGravityProcessing,
                                                       kFunctionalMovementPreparation,
                                                       kFunctionalMovementCapture,
                                                       kFunctionalReturnPosePreparation,
                                                       kFunctionalReturnPoseCapture,
                                                       kFunctionalProcessing,
                                                       kCountdown};
} // namespace guidance

/**
 * Writes line-oriented FRONTEND protocol events consumed by the PySide6
 * application.
 * @class EventWriter.
 */
class EventWriter {
  public:
    /**
   * Creates a writer that emits events to the supplied stream.
   * @param output Stream that receives protocol lines and is flushed after each
   * event.
   */
    explicit EventWriter(std::ostream& output);

    /**
   * Emits a backend state transition.
   * @param state Stable state identifier understood by the frontend.
   */
    void emitState(std::string_view state) const;

    /**
   * Emits the result of a completed operation.
   * @param result Stable result identifier understood by the frontend.
   * @param detail Optional human-readable or structured result detail.
   */
    void emitResult(std::string_view result, std::string_view detail = {}) const;

    /**
   * Emits an error event without throwing an exception.
   * @param error Stable error identifier understood by the frontend.
   * @param detail Optional explanation displayed to the clinician.
   */
    void emitError(std::string_view error, std::string_view detail = {}) const;

    /**
   * Emits clinician guidance for the current workflow phase.
   * @param phase Stable guidance phase identifier.
   * @param detail Optional instruction or phase-specific payload.
   */
    void emitGuidance(std::string_view phase, std::string_view detail = {}) const;

    /**
   * Emits one countdown event per second until the requested interval expires.
   * @param phase Workflow phase included in each countdown event.
   * @param seconds Number of whole seconds to count down.
   */
    void runGuidedCountdown(std::string_view phase, int seconds) const;

    /**
   * Formats one protocol event without writing it.
   * @param category Top-level event category such as STATE, RESULT, ERROR, or
   * GUIDANCE.
   * @param name Stable event identifier within the category.
   * @param detail Optional payload appended as the fourth protocol field.
   * @return A FRONTEND-delimited protocol line without a trailing newline.
   */
    static std::string formatEvent(std::string_view category, std::string_view name,
                                   std::string_view detail = {});

  private:
    /**
   * Formats, writes, and flushes one protocol event.
   * @param category Top-level event category.
   * @param name Stable event identifier.
   * @param detail Optional event payload.
   */
    void emitEvent(std::string_view category, std::string_view name,
                   std::string_view detail = {}) const;

    std::ostream& output_;
};

} // namespace frontend_protocol
