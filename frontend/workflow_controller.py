"""Non-visual state transitions for the clinician workflow."""

from __future__ import annotations

from dataclasses import dataclass

from frontend.protocol import (
    Category,
    ErrorName,
    FrontendProtocolEvent,
    ResultName,
    SensorStatus,
    StateName,
)
from frontend.sensor_mapping import CANONICAL_SEGMENTS, EXPECTED_SENSOR_COUNT
from frontend.session_state import SessionState


@dataclass(frozen=True, slots=True)
class WorkflowUpdate:
    """Describe the state change caused by one backend event."""

    event: FrontendProtocolEvent
    state_changed: bool = True
    backend_state: str | None = None
    request_sensor_continue: bool = False
    request_viewer_stop: bool = False


class WorkflowController:
    """Own session state and enforce workflow transition prerequisites."""

    def __init__(self) -> None:
        self.state = SessionState.for_segments(CANONICAL_SEGMENTS)
        self.backend_running = False
        self.viewer_running = False
        self.model_running = False

    def reset_for_backend_start(self) -> None:
        self.state.begin_backend_start(CANONICAL_SEGMENTS)
        self.backend_running = True

    def mark_backend_stopped(self) -> None:
        self.state.mark_backend_stopped()
        self.backend_running = False

    def update_process_states(
        self, *, backend: bool, viewer: bool, model: bool
    ) -> None:
        self.backend_running = backend
        self.viewer_running = viewer
        self.model_running = model

    def can_confirm_session(self) -> bool:
        return (
            self.backend_running
            and self.state.backend_state == "session"
            and self.state.connected_sensor_count == EXPECTED_SENSOR_COUNT
            and self.state.model_prepared
            and not self.state.recording_active
        )

    def can_start_static_calibration(self) -> bool:
        return self.backend_running and self.state.backend_state == "menu"

    def can_start_functional_calibration(self) -> bool:
        return (
            self.can_start_static_calibration()
            and self.state.static_calibrated
            and self.state.pending_functional_choice is None
        )

    def can_start_recording(self) -> bool:
        return (
            self.can_start_static_calibration()
            and self.state.static_calibrated
            and self.state.connected_sensor_count == EXPECTED_SENSOR_COUNT
            and self.state.model_prepared
            and not self.model_running
        )

    def apply_backend_event(self, event: FrontendProtocolEvent) -> WorkflowUpdate:
        state = self.state
        category, name, detail = event.category, event.name, event.detail
        request_sensor_continue = False
        request_viewer_stop = False

        if category == Category.STATE:
            state_mapping = {
                StateName.SENSORS: "sensors",
                StateName.HEADING: "heading",
                StateName.SESSION: "session",
                StateName.MENU: "menu",
                StateName.JOINT: "joint",
                StateName.RECORDING: "recording",
                StateName.EXITING: "busy",
            }
            backend_state = state_mapping.get(name)
            if backend_state is None:
                return WorkflowUpdate(event, state_changed=False)
            state.backend_state = backend_state
            if backend_state == "menu":
                state.pending_functional_choice = None
            return WorkflowUpdate(event, backend_state=backend_state)

        if category == Category.RESULT and name == ResultName.SENSOR_STATUS:
            try:
                sensor_id, sensor_status = detail.rsplit(":", 1)
            except ValueError:
                return WorkflowUpdate(event, state_changed=False)
            state.sensor_status_events_received = True
            segment = next(
                (
                    segment
                    for segment, mapped_id in state.sensor_mapping.items()
                    if mapped_id.upper() == sensor_id.upper()
                ),
                None,
            )
            if segment is not None:
                state.sensor_connection_status[segment] = (
                    sensor_status == SensorStatus.CONNECTED
                )
                state.connected_sensor_count = sum(
                    state.sensor_connection_status.values()
                )
            elif sensor_status == SensorStatus.CONNECTED:
                state.unmapped_connected_sensors.add(sensor_id)
            else:
                state.unmapped_connected_sensors.discard(sensor_id)
            request_sensor_continue = self._should_continue_sensors()
        elif category == Category.RESULT and name == ResultName.UNMAPPED_SENSOR:
            if detail:
                state.unmapped_connected_sensors.add(detail)
        elif category == Category.RESULT and name == ResultName.SENSORS_CONNECTED:
            if not state.sensor_status_events_received:
                try:
                    state.connected_sensor_count = int(detail)
                except ValueError:
                    state.connected_sensor_count = 0
            request_sensor_continue = self._should_continue_sensors()
        elif category == Category.RESULT and name == ResultName.SESSION_CONFIGURED:
            state.confirmed_session_name = detail or state.confirmed_session_name
        elif category == Category.RESULT and name == ResultName.STATIC_CALIBRATION:
            state.functional_calibrations.clear()
            state.failed_functional_calibrations.clear()
            state.pending_functional_choice = None
            state.static_calibrated = True
            state.static_pose = detail.split(":", 1)[0]
            state.static_pose_preset = state.pending_static_pose_preset
            state.pending_static_pose_preset = None
        elif category == Category.RESULT and name == ResultName.FUNCTIONAL_CALIBRATION:
            if detail:
                state.functional_calibrations.add(detail)
                state.failed_functional_calibrations.pop(detail, None)
        elif category == Category.RESULT and name == ResultName.RECORDING_STARTED:
            state.recording_active = True
        elif category == Category.RESULT and name == ResultName.RECORDING_STOPPED:
            state.recording_active = False
            request_viewer_stop = state.pending_viewer_stop
            state.pending_viewer_stop = False
        elif category == Category.ERROR and name == ErrorName.STATIC_CALIBRATION:
            state.invalidate_calibration()
        elif category == Category.ERROR and name == ErrorName.FUNCTIONAL_CALIBRATION:
            if ":" in detail:
                joint_name, reason = detail.split(":", 1)
                state.failed_functional_calibrations[joint_name] = reason
        elif category == Category.ERROR and name == ErrorName.RECORDING:
            state.recording_active = False
            state.pending_recording_start = False

        return WorkflowUpdate(
            event,
            request_sensor_continue=request_sensor_continue,
            request_viewer_stop=request_viewer_stop,
        )

    def mark_viewer_ready(self) -> bool:
        self.state.viewer_ready = True
        should_start_recording = self.state.pending_recording_start
        self.state.pending_recording_start = False
        return should_start_recording

    def mark_viewer_stream_ended(self) -> None:
        self.state.viewer_ready = False

    def mark_viewer_closed(self) -> None:
        self.state.viewer_ready = False
        self.state.pending_recording_start = False
        self.state.pending_viewer_stop = False

    def mark_viewer_finished(self) -> tuple[bool, bool]:
        intentional_stop = self.state.viewer_stop_requested
        was_waiting = self.state.pending_recording_start
        self.state.reset_viewer()
        self.viewer_running = False
        return intentional_stop, was_waiting

    def _should_continue_sensors(self) -> bool:
        state = self.state
        return (
            self.backend_running
            and not state.sensor_continue_sent
            and state.connected_sensor_count == EXPECTED_SENSOR_COUNT
            and state.backend_state == "sensors"
        )
