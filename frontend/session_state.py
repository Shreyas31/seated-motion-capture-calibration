from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable

ModelInputSignature = tuple[
    str,
    float,
    float,
]


@dataclass(slots=True)
class SessionState:
    """Mutable non-visual state for one clinician application session."""

    backend_state: str = "stopped"
    last_guided_backend_state: str = "stopped"
    confirmed_session_name: str | None = None

    connected_sensor_count: int = 0
    sensor_status_events_received: bool = False
    sensor_continue_sent: bool = False
    sensor_mapping: dict[str, str] = field(default_factory=dict)
    sensor_connection_status: dict[str, bool] = field(default_factory=dict)
    unmapped_connected_sensors: set[str] = field(default_factory=set)

    static_calibrated: bool = False
    static_pose: str | None = None
    static_pose_preset: str | None = None
    pending_static_pose_preset: str | None = None
    pending_functional_choice: int | None = None
    functional_calibrations: set[str] = field(default_factory=set)
    failed_functional_calibrations: dict[str, str] = field(default_factory=dict)

    recording_active: bool = False

    viewer_ready: bool = False
    viewer_joint_angle_output: Path | None = None
    viewer_stop_requested: bool = False
    pending_recording_start: bool = False
    pending_viewer_stop: bool = False

    current_model_file: Path | None = None
    current_geometry_directory: Path | None = None
    model_prepared: bool = False
    pending_model_output: Path | None = None
    pending_model_inputs: ModelInputSignature | None = None
    prepared_model_inputs: ModelInputSignature | None = None

    @classmethod
    def for_segments(cls, segments: Iterable[str]) -> "SessionState":
        return cls(sensor_connection_status={segment: False for segment in segments})

    def invalidate_calibration(self) -> None:
        self.static_calibrated = False
        self.static_pose = None
        self.static_pose_preset = None
        self.pending_static_pose_preset = None
        self.pending_functional_choice = None
        self.functional_calibrations.clear()
        self.failed_functional_calibrations.clear()

    def invalidate_model(self) -> None:
        self.current_model_file = None
        self.current_geometry_directory = None
        self.model_prepared = False
        self.prepared_model_inputs = None

    def clear_pending_model_generation(self) -> None:
        self.pending_model_output = None
        self.pending_model_inputs = None

    def reset_model(self) -> None:
        self.invalidate_model()
        self.clear_pending_model_generation()

    def reset_viewer(self) -> None:
        self.viewer_ready = False
        self.viewer_joint_angle_output = None
        self.viewer_stop_requested = False
        self.pending_recording_start = False
        self.pending_viewer_stop = False

    def begin_backend_start(self, segments: Iterable[str]) -> None:
        self.backend_state = "starting"
        self.confirmed_session_name = None
        self.connected_sensor_count = 0
        self.sensor_status_events_received = False
        self.sensor_continue_sent = False
        self.sensor_connection_status = {segment: False for segment in segments}
        self.unmapped_connected_sensors.clear()
        self.invalidate_calibration()
        self.recording_active = False
        self.reset_model()
        self.pending_recording_start = False
        self.pending_viewer_stop = False

    def mark_backend_stopped(self) -> None:
        self.backend_state = "stopped"
        self.last_guided_backend_state = "stopped"
        self.confirmed_session_name = None
        self.connected_sensor_count = 0
        self.sensor_status_events_received = False
        self.sensor_continue_sent = False
        self.sensor_connection_status = {
            segment: False for segment in self.sensor_connection_status
        }
        self.unmapped_connected_sensors.clear()
        self.invalidate_calibration()
        self.recording_active = False
        self.reset_model()
        self.reset_viewer()
