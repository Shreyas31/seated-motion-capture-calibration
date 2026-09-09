from frontend.sensor_mapping import EXPECTED_SENSOR_COUNT
from frontend.workflow_ui import build_diagnostics_page


def test_session_status_defaults(qtbot):
    ui = build_diagnostics_page()
    qtbot.addWidget(ui.session_status_group)

    assert ui.status_label.text() == "Status: Sensor system not started"
    assert ui.guidance_label.text() == (
        "Guidance: Start the sensor system, enter the session details, "
        "generate the patient model, and confirm the session."
    )
    assert f"Sensors 0/{EXPECTED_SENSOR_COUNT}" in ui.compact_session_label.text()
    assert ui.sensor_summary_label.text() == "Sensors: not checked"
    assert ui.static_summary_label.text().endswith("not completed")
    assert ui.recording_summary_label.text() == "Recording: stopped"


def test_diagnostic_logs_are_read_only(qtbot):
    ui = build_diagnostics_page()
    qtbot.addWidget(ui.backend_log)
    qtbot.addWidget(ui.viewer_log)

    assert ui.backend_log.isReadOnly()
    assert ui.viewer_log.isReadOnly()
