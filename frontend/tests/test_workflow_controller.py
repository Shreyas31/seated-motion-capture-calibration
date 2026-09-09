from frontend.protocol import FrontendProtocolEvent
from frontend.sensor_mapping import CANONICAL_SEGMENTS, EXPECTED_SENSOR_COUNT
from frontend.workflow_controller import WorkflowController


def test_backend_results_update_non_visual_session_state():
    workflow = WorkflowController()

    workflow.apply_backend_event(
        FrontendProtocolEvent("RESULT", "SENSORS_CONNECTED", "12")
    )
    workflow.apply_backend_event(
        FrontendProtocolEvent(
            "RESULT", "STATIC_CALIBRATION", f"Chair:{EXPECTED_SENSOR_COUNT}"
        )
    )
    workflow.apply_backend_event(
        FrontendProtocolEvent("RESULT", "FUNCTIONAL_CALIBRATION", "Right knee")
    )

    assert workflow.state.connected_sensor_count == 12
    assert workflow.state.static_calibrated
    assert workflow.state.static_pose == "Chair"
    assert workflow.state.functional_calibrations == {"Right knee"}


def test_detailed_sensor_events_use_mapping_and_request_continuation():
    workflow = WorkflowController()
    workflow.backend_running = True
    workflow.state.backend_state = "sensors"
    workflow.state.sensor_mapping = {
        segment: f"sensor-{index}" for index, segment in enumerate(CANONICAL_SEGMENTS)
    }

    update = None
    for index in range(EXPECTED_SENSOR_COUNT):
        update = workflow.apply_backend_event(
            FrontendProtocolEvent(
                "RESULT", "SENSOR_STATUS", f"SENSOR-{index}:CONNECTED"
            )
        )

    assert workflow.state.connected_sensor_count == EXPECTED_SENSOR_COUNT
    assert update is not None and update.request_sensor_continue


def test_action_gates_are_explicit():
    workflow = WorkflowController()
    workflow.update_process_states(backend=True, viewer=False, model=False)
    workflow.state.backend_state = "session"
    workflow.state.connected_sensor_count = EXPECTED_SENSOR_COUNT
    workflow.state.model_prepared = True

    assert workflow.can_confirm_session()
    assert not workflow.can_start_static_calibration()

    workflow.state.backend_state = "menu"
    workflow.state.static_calibrated = True

    assert workflow.can_start_static_calibration()
    assert workflow.can_start_functional_calibration()
    assert workflow.can_start_recording()


def test_backend_restart_resets_session_state():
    workflow = WorkflowController()
    workflow.state.connected_sensor_count = EXPECTED_SENSOR_COUNT
    workflow.state.static_calibrated = True
    workflow.state.recording_active = True

    workflow.reset_for_backend_start()

    assert workflow.backend_running
    assert workflow.state.backend_state == "starting"
    assert workflow.state.connected_sensor_count == 0
    assert not workflow.state.static_calibrated
    assert not workflow.state.recording_active
