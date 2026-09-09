import pytest
from PySide6.QtWidgets import QApplication, QMessageBox

from frontend.app import MainWindow, load_application_stylesheet
from frontend.sensor_mapping import EXPECTED_SENSOR_COUNT


@pytest.fixture
def window(qtbot):
    load_application_stylesheet(QApplication.instance())
    frontend = MainWindow()
    qtbot.addWidget(frontend)
    return frontend


def test_sensor_count_success(window):
    window.handle_backend_event(
        "RESULT",
        "SENSORS_CONNECTED",
        str(EXPECTED_SENSOR_COUNT),
    )

    assert window.workflow.state.connected_sensor_count == EXPECTED_SENSOR_COUNT
    assert (
        f"{EXPECTED_SENSOR_COUNT}/{EXPECTED_SENSOR_COUNT}"
        in window.status_ui.sensor_summary_label.text()
    )


def test_incomplete_sensor_count_is_reported(window):
    window.handle_backend_event(
        "RESULT",
        "SENSORS_CONNECTED",
        "14",
    )

    assert window.workflow.state.connected_sensor_count == 14
    assert f"14/{EXPECTED_SENSOR_COUNT}" in window.status_ui.sensor_summary_label.text()
    assert "#8b1a1a" in (window.status_ui.sensor_summary_label.styleSheet())


def test_static_calibration_success(window):
    window.handle_backend_event(
        "RESULT",
        "STATIC_CALIBRATION",
        f"Chair:{EXPECTED_SENSOR_COUNT}",
    )

    assert window.workflow.state.static_calibrated is True
    assert window.workflow.state.static_pose == "Chair"
    assert "completed" in (window.status_ui.static_summary_label.text())


def test_static_calibration_failure(window):
    window.handle_backend_event(
        "RESULT",
        "STATIC_CALIBRATION",
        f"Chair:{EXPECTED_SENSOR_COUNT}",
    )

    window.handle_backend_event(
        "ERROR",
        "STATIC_CALIBRATION",
        f"Expected {EXPECTED_SENSOR_COUNT} sensors but accepted 15",
    )

    assert window.workflow.state.static_calibrated is False
    assert window.workflow.state.static_pose is None
    assert "not completed" in (window.status_ui.static_summary_label.text())


def test_functional_calibration_success(window):
    window.handle_backend_event(
        "RESULT",
        "FUNCTIONAL_CALIBRATION",
        "Right knee",
    )

    assert "Right knee" in window.workflow.state.functional_calibrations
    assert "Right knee" in (window.status_ui.functional_summary_label.text())
    assert "passed" in (window.status_ui.functional_summary_label.text())


def test_functional_calibration_failure(window):
    window.handle_backend_event(
        "ERROR",
        "FUNCTIONAL_CALIBRATION",
        "Right knee:Functional axis estimate rejected",
    )

    assert (
        window.workflow.state.failed_functional_calibrations["Right knee"]
        == "Functional axis estimate rejected"
    )

    summary = window.status_ui.functional_summary_label.text()

    assert "Right knee" in summary
    assert "failed" in summary
    assert "#8b1a1a" in summary


def test_success_clears_previous_functional_failure(
    window,
):
    window.handle_backend_event(
        "ERROR",
        "FUNCTIONAL_CALIBRATION",
        "Left elbow flexion/extension:Missing dynamic samples",
    )

    window.handle_backend_event(
        "RESULT",
        "FUNCTIONAL_CALIBRATION",
        "Left elbow flexion/extension",
    )

    assert (
        "Left elbow flexion/extension" in window.workflow.state.functional_calibrations
    )
    assert "Left elbow flexion/extension" not in (
        window.workflow.state.failed_functional_calibrations
    )


def test_joint_state_sends_pending_functional_choice_once(
    window,
    monkeypatch,
):
    sent_values = []

    monkeypatch.setattr(
        window,
        "send_backend_value",
        sent_values.append,
    )

    window.workflow.state.backend_state = "joint"
    window.workflow.state.pending_functional_choice = 3

    window.send_pending_functional_choice()
    window.send_pending_functional_choice()

    assert sent_values == ["3"]
    assert window.workflow.state.pending_functional_choice is None


def test_pending_functional_choice_waits_for_joint_state(
    window,
    monkeypatch,
):
    sent_values = []

    monkeypatch.setattr(
        window,
        "send_backend_value",
        sent_values.append,
    )

    window.workflow.state.backend_state = "menu"
    window.workflow.state.pending_functional_choice = 3

    window.send_pending_functional_choice()

    assert sent_values == []
    assert window.workflow.state.pending_functional_choice == 3


def test_recording_state(window):
    window.handle_backend_event(
        "RESULT",
        "RECORDING_STARTED",
        "P001_Measurement_1.csv",
    )

    assert window.workflow.state.recording_active is True
    assert "active" in (window.status_ui.recording_summary_label.text())

    window.handle_backend_event(
        "RESULT",
        "RECORDING_STOPPED",
        "P001_Measurement_1.csv",
    )

    assert window.workflow.state.recording_active is False
    assert "stopped" in (window.status_ui.recording_summary_label.text())


def test_measurement_change_invalidates_model(
    window,
):
    window.patient_ui.session_name_input.setText("P001")
    window.patient_ui.height_input.setValue(1.70)

    signature = window.patient_input_signature()

    window.workflow.state.model_prepared = True
    window.workflow.state.prepared_model_inputs = signature
    window.workflow.state.current_model_file = object()
    window.workflow.state.current_geometry_directory = object()

    window.patient_ui.height_input.setValue(1.80)

    assert window.workflow.state.model_prepared is False
    assert window.workflow.state.current_model_file is None
    assert window.workflow.state.current_geometry_directory is None
    assert "regeneration required" in (window.patient_ui.model_summary_label.text())


def test_guidance_countdown(window):
    window.handle_backend_guidance(
        "COUNTDOWN",
        "STATIC:3",
    )

    assert "starts in 3" in (window.status_ui.guidance_label.text())


def test_guidance_static_capture(window):
    window.handle_backend_guidance(
        "STATIC_CAPTURE",
        "5",
    )

    guidance = window.status_ui.guidance_label.text()

    assert "5 seconds" in guidance
    assert "Do not move" in guidance


def test_guidance_functional_gravity_capture_includes_duration(window):
    window.handle_backend_guidance(
        "FUNCTIONAL_GRAVITY_CAPTURE",
        "Right elbow|5",
    )

    guidance = window.status_ui.guidance_label.text()

    assert "Right elbow" in guidance
    assert "5 seconds" in guidance
    assert "Do not move" in guidance


def test_guidance_functional_movement_capture_includes_duration(window):
    window.handle_backend_guidance(
        "FUNCTIONAL_MOVEMENT_CAPTURE",
        "Right elbow|10",
    )

    guidance = window.status_ui.guidance_label.text()

    assert "Right elbow" in guidance
    assert "10 seconds" in guidance
    assert "Continue moving smoothly" in guidance


def test_guidance_functional_return_pose_capture_includes_duration(window):
    window.handle_backend_guidance(
        "FUNCTIONAL_RETURN_POSE_CAPTURE",
        "Right elbow|5",
    )

    guidance = window.status_ui.guidance_label.text()

    assert "Right elbow" in guidance
    assert "5 seconds" in guidance
    assert "Do not move" in guidance


def test_workflow_has_expected_pages(window):
    assert window.workflow_shell.workflow_stack.count() == 4
    assert window.workflow_shell.workflow_stack.currentIndex() == 0

    assert [button.text() for button in window.workflow_shell.workflow_buttons] == [
        "1  Setup",
        "2  Calibration",
        "3  Recording",
        "Diagnostics",
    ]


def test_sensor_system_start_is_on_setup_page(
    window,
):
    setup_page = window.workflow_shell.workflow_stack.widget(0)
    calibration_page = window.workflow_shell.workflow_stack.widget(1)

    assert setup_page.isAncestorOf(window.patient_ui.start_backend_button)
    assert not calibration_page.isAncestorOf(window.patient_ui.start_backend_button)
    assert window.patient_ui.start_backend_button.text() == "Start sensor system"


def test_confirm_session_is_on_setup_page(window):
    setup_page = window.workflow_shell.workflow_stack.widget(0)
    calibration_page = window.workflow_shell.workflow_stack.widget(1)

    assert setup_page.isAncestorOf(window.patient_ui.submit_session_button)
    assert not calibration_page.isAncestorOf(window.patient_ui.submit_session_button)


def test_all_workflow_pages_remain_available(window):
    window.update_controls()

    assert all(button.isEnabled() for button in window.workflow_shell.workflow_buttons)


def test_confirm_session_remains_disabled_until_backend_is_ready(
    window,
):
    window.patient_ui.session_name_input.setText("P001")
    window.workflow.state.model_prepared = True
    window.workflow.state.connected_sensor_count = EXPECTED_SENSOR_COUNT
    window.workflow.state.backend_state = "sensors"

    window.update_controls()

    assert not window.patient_ui.submit_session_button.isEnabled()


def test_backend_confirmation_locks_session_name(window):
    window.patient_ui.session_name_input.setText("P001")

    window.handle_backend_event(
        "RESULT",
        "SESSION_CONFIGURED",
        "P001",
    )

    assert window.workflow.state.confirmed_session_name == "P001"
    assert not window.patient_ui.session_name_input.isEnabled()
    assert "locked after confirmation" in window.patient_ui.session_name_input.toolTip()


def test_backend_finish_restores_clean_new_session(window):
    window.patient_ui.session_name_input.setText("P001")
    window.patient_ui.pose_box.setCurrentIndex(1)
    window.patient_ui.height_input.setValue(1.82)
    window.patient_ui.foot_length_input.setValue(0.29)
    window.recording_ui.stationary_feet_checkbox.setChecked(True)
    window.workflow.state.confirmed_session_name = "P001"
    window.workflow.state.connected_sensor_count = EXPECTED_SENSOR_COUNT
    window.workflow.state.static_calibrated = True
    window.workflow.state.static_pose = "Chair"
    window.workflow.state.functional_calibrations.add("Right knee")
    window.workflow.state.model_prepared = True

    window.backend_finished(0, None)

    assert window.workflow.state.backend_state == "stopped"
    assert window.workflow.state.connected_sensor_count == 0
    assert not window.workflow.state.static_calibrated
    assert window.workflow.state.functional_calibrations == set()
    assert not window.workflow.state.model_prepared
    assert window.patient_ui.session_name_input.text() == ""
    assert window.patient_ui.pose_box.currentData() == "chair"
    assert window.patient_ui.height_input.value() == pytest.approx(1.70)
    assert window.patient_ui.foot_length_input.value() == pytest.approx(0.25)
    assert not window.recording_ui.stationary_feet_checkbox.isChecked()
    assert (
        window.calibration_ui.static_calibration_status_label.text() == "Not performed"
    )
    assert (
        window.patient_ui.model_summary_label.text() == "Patient model: not generated"
    )
    assert window.patient_ui.start_backend_button.isEnabled()


def test_confirm_session_warns_when_patient_model_is_missing(window, monkeypatch):
    warnings = []
    monkeypatch.setattr(
        QMessageBox,
        "warning",
        lambda _parent, title, message: warnings.append((title, message)),
    )
    window.patient_ui.session_name_input.setText("P001")
    window.workflow.state.connected_sensor_count = EXPECTED_SENSOR_COUNT

    window.submit_session_name()

    assert warnings
    assert warnings[-1][0] == "Patient model not prepared"


def test_recording_warns_when_static_calibration_is_missing(window, monkeypatch):
    warnings = []
    monkeypatch.setattr(
        QMessageBox,
        "warning",
        lambda _parent, title, message: warnings.append((title, message)),
    )
    window.workflow.state.model_prepared = True
    window.workflow.state.connected_sensor_count = EXPECTED_SENSOR_COUNT

    window.start_recording()

    assert warnings
    assert warnings[-1][0] == "Static calibration required"


def test_sensor_overview_uses_aggregate_count(window):
    window.handle_backend_event(
        "RESULT",
        "SENSORS_CONNECTED",
        "14",
    )

    assert window.workflow.state.connected_sensor_count == 14
    assert f"14 of {EXPECTED_SENSOR_COUNT}" in (
        window.patient_ui.sensor_connection_overview_label.text()
    )


def test_calibration_cards_follow_results(window):
    window.handle_backend_event(
        "RESULT",
        "STATIC_CALIBRATION",
        f"Chair:{EXPECTED_SENSOR_COUNT}",
    )
    window.handle_backend_event(
        "ERROR",
        "FUNCTIONAL_CALIBRATION",
        "Left elbow flexion/extension:Insufficient movement",
    )

    assert "Passed" in (window.calibration_ui.static_calibration_status_label.text())
    assert "Failed" in (
        window.calibration_ui.functional_status_labels[
            "Left elbow flexion/extension"
        ].text()
    )


def test_recording_primary_button_follows_state(window):
    window.handle_backend_event(
        "RESULT",
        "RECORDING_STARTED",
        "P001_Measurement_1.csv",
    )

    assert window.recording_ui.recording_primary_button.text() == "Stop recording"
    assert window.recording_ui_timer.isActive()

    window.handle_backend_event(
        "RESULT",
        "RECORDING_STOPPED",
        "P001_Measurement_1.csv",
    )
    window.workflow.state.backend_state = "menu"
    window.update_controls()

    assert window.recording_ui.recording_primary_button.text() == "Start recording"
    assert not window.recording_ui_timer.isActive()


def test_intentional_viewer_finish_uses_session_state(window):
    window.workflow.state.viewer_ready = True
    window.workflow.state.viewer_stop_requested = True
    window.workflow.state.pending_recording_start = False

    window.viewer_finished(1, None)

    assert window.status_ui.status_label.text() == "OpenSim viewer stopped"
    assert not window.workflow.state.viewer_ready
    assert not window.workflow.state.viewer_stop_requested


def test_sensor_continuation_is_sent_once(
    window,
    monkeypatch,
):
    sent_values = []

    monkeypatch.setattr(
        window,
        "backend_is_running",
        lambda: True,
    )
    monkeypatch.setattr(
        window,
        "send_backend_value",
        sent_values.append,
    )

    window.workflow.state.backend_state = "sensors"
    window.workflow.state.connected_sensor_count = EXPECTED_SENSOR_COUNT

    window.continue_when_all_sensors_connected()
    window.continue_when_all_sensors_connected()

    assert sent_values == [""]
    assert window.workflow.state.sensor_continue_sent


def test_sensor_continuation_waits_for_all_sensors(
    window,
    monkeypatch,
):
    sent_values = []

    monkeypatch.setattr(
        window,
        "backend_is_running",
        lambda: True,
    )
    monkeypatch.setattr(
        window,
        "send_backend_value",
        sent_values.append,
    )

    window.workflow.state.backend_state = "sensors"
    window.workflow.state.connected_sensor_count = 16

    window.continue_when_all_sensors_connected()

    assert sent_values == []
    assert not window.workflow.state.sensor_continue_sent


def test_exit_cancel_leaves_shutdown_inactive(
    window,
    monkeypatch,
):
    monkeypatch.setattr(
        QMessageBox,
        "question",
        lambda *_args, **_kwargs: QMessageBox.StandardButton.No,
    )

    window.request_application_exit()

    assert not window.shutdown_requested
    assert not window.shutdown_complete


def test_exit_warning_mentions_active_recording(
    window,
):
    window.workflow.state.recording_active = True

    message = window.application_exit_warning()

    assert "recording" in message.lower()
    assert "CSV" in message


def test_shutdown_finishes_when_no_processes_are_active(
    window,
    monkeypatch,
):
    monkeypatch.setattr(
        window,
        "managed_processes_are_active",
        lambda: False,
    )

    closed = []
    monkeypatch.setattr(
        window,
        "close",
        lambda: closed.append(True),
    )

    window.shutdown_requested = True
    window.finish_application_shutdown_if_ready()

    assert window.shutdown_complete
    assert closed == [True]
