from pathlib import Path

from frontend.sensor_mapping import EXPECTED_SENSOR_COUNT
from frontend.session_state import SessionState

SEGMENTS = ("Pelvis", "Sternum", "Head")


def test_session_state_starts_in_safe_defaults():
    state = SessionState.for_segments(SEGMENTS)

    assert state.backend_state == "stopped"
    assert not state.static_calibrated
    assert not state.recording_active
    assert not state.viewer_ready
    assert state.sensor_connection_status == {segment: False for segment in SEGMENTS}


def test_mutable_collections_are_not_shared():
    first = SessionState.for_segments(SEGMENTS)
    second = SessionState.for_segments(SEGMENTS)

    first.sensor_mapping["Pelvis"] = "sensor-1"
    first.functional_calibrations.add("Right knee")
    first.failed_functional_calibrations["Left knee"] = "rejected"
    first.unmapped_connected_sensors.add("unknown")

    assert second.sensor_mapping == {}
    assert second.functional_calibrations == set()
    assert second.failed_functional_calibrations == {}
    assert second.unmapped_connected_sensors == set()


def test_invalidate_calibration_clears_results_and_pending_choices():
    state = SessionState.for_segments(SEGMENTS)
    state.static_calibrated = True
    state.static_pose = "chair"
    state.static_pose_preset = "chair_palms_together"
    state.pending_static_pose_preset = "bed_palms_together"
    state.functional_calibrations.add("Right knee")
    state.failed_functional_calibrations["Left knee"] = "rejected"
    state.pending_functional_choice = 3
    state.invalidate_calibration()
    assert state.pending_functional_choice is None
    assert not state.static_calibrated
    assert state.static_pose is None
    assert state.static_pose_preset is None
    assert state.pending_static_pose_preset is None
    assert state.functional_calibrations == set()
    assert state.failed_functional_calibrations == {}


def test_begin_backend_start_resets_previous_session_data():
    state = SessionState.for_segments(SEGMENTS)
    state.backend_state = "recording"
    state.confirmed_session_name = "PreviousSession"
    state.recording_active = True
    state.connected_sensor_count = 3
    state.sensor_connection_status["Pelvis"] = True
    state.model_prepared = True
    state.current_model_file = Path("subject.osim")
    state.static_calibrated = True

    state.begin_backend_start(SEGMENTS)

    assert state.backend_state == "starting"
    assert state.confirmed_session_name is None
    assert not state.recording_active
    assert state.connected_sensor_count == 0
    assert not any(state.sensor_connection_status.values())
    assert not state.model_prepared
    assert state.current_model_file is None
    assert not state.static_calibrated


def test_backend_stop_clears_pending_functional_choice():
    state = SessionState()
    state.pending_functional_choice = 5
    state.confirmed_session_name = "P001"
    state.connected_sensor_count = EXPECTED_SENSOR_COUNT
    state.sensor_connection_status = {"Pelvis": True}
    state.unmapped_connected_sensors.add("unknown")
    state.static_calibrated = True
    state.functional_calibrations.add("Right knee")
    state.model_prepared = True
    state.current_model_file = Path("patient.osim")
    state.viewer_ready = True
    state.recording_active = True

    state.mark_backend_stopped()

    assert state.pending_functional_choice is None
    assert state.confirmed_session_name is None
    assert state.connected_sensor_count == 0
    assert not any(state.sensor_connection_status.values())
    assert state.unmapped_connected_sensors == set()
    assert not state.static_calibrated
    assert state.functional_calibrations == set()
    assert not state.model_prepared
    assert state.current_model_file is None
    assert not state.viewer_ready
    assert not state.recording_active


def test_reset_viewer_clears_transient_viewer_state():
    state = SessionState.for_segments(SEGMENTS)
    state.viewer_ready = True
    state.viewer_stop_requested = True
    state.pending_recording_start = True
    state.pending_viewer_stop = True
    state.viewer_joint_angle_output = Path("angles.csv")
    state.reset_viewer()

    assert not state.viewer_ready
    assert not state.viewer_stop_requested
    assert not state.pending_recording_start
    assert not state.pending_viewer_stop
    assert state.viewer_joint_angle_output is None
