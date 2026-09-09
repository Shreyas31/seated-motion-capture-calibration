import pytest
from PySide6.QtWidgets import (
    QApplication,
    QCheckBox,
    QGroupBox,
    QLabel,
    QPushButton,
    QSizePolicy,
)

from frontend.app import MainWindow
from frontend.application_bootstrap import load_application_stylesheet
from frontend.sensor_mapping import EXPECTED_SENSOR_COUNT


@pytest.fixture
def window(qtbot):
    frontend = MainWindow()
    qtbot.addWidget(frontend)
    return frontend


def test_window_and_workflow_page_contract(window):
    assert window.windowTitle() == "IRP Seated Motion Capture"
    assert (window.width(), window.height()) == (1000, 700)
    assert window.workflow_shell.workflow_stack.count() == 4
    assert window.workflow_shell.workflow_stack.currentIndex() == 0
    assert [button.text() for button in window.workflow_shell.workflow_buttons] == [
        "1  Setup",
        "2  Calibration",
        "3  Recording",
        "Diagnostics",
    ]


def test_initial_visible_text_contract(window):
    button_text = [button.text() for button in window.findChildren(QPushButton)]
    assert button_text == [
        "1  Setup",
        "2  Calibration",
        "3  Recording",
        "Diagnostics",
        "Preview calibrated pose",
        "Stop preview",
        "Start recording",
        "Exit sensor system safely",
        "Perform static calibration",
        "Start selected functional calibration",
        "Start sensor system",
        "Run system check",
        "Reset heading offsets",
        "View all sensors",
        "Reload sensor mapping",
        "Configure sensor mapping",
        "Generate patient OpenSim model",
        "Confirm session",
    ]
    group_titles = [group.title() for group in window.findChildren(QGroupBox)]
    assert group_titles == [
        "Current step",
        "Current session status",
        "Recording",
        "Calibration workflow",
        "Required static calibration",
        "Optional functional calibrations",
        "Sensor system",
        "Sensor-to-segment mapping",
        "Session details",
    ]
    assert [box.text() for box in window.findChildren(QCheckBox)] == [
        "Keep feet stationary and estimate pelvis translation"
    ]
    label_text = [label.text() for label in window.findChildren(QLabel)]
    for required_text in (
        "IRP Seated Motion Capture",
        "Session: not set",
        f"Sensors: 0/{EXPECTED_SENSOR_COUNT}",
        "Sensor system: stopped",
        "Viewer: stopped",
        "Status: Sensor system not started",
        "Ready to record",
        "00:00",
        "OpenSim viewer: stopped",
        "No recording has been made in this session.",
        "Calibration pose:",
        "Patient height:",
        "Foot length:",
        "Session name:",
        "Patient model: not generated",
    ):
        assert required_text in label_text


def test_combo_defaults_values_and_order(window):
    assert window.patient_ui.pose_box.currentIndex() == 0
    assert [
        (
            window.patient_ui.pose_box.itemText(index),
            window.patient_ui.pose_box.itemData(index),
        )
        for index in range(window.patient_ui.pose_box.count())
    ] == [("Chair seated", "chair"), ("Bed seated", "bed")]
    assert window.calibration_ui.joint_box.currentIndex() == 0
    assert window.calibration_ui.joint_box.currentData() == 1
    assert window.patient_ui.height_input.value() == pytest.approx(1.70)
    assert window.patient_ui.foot_length_input.value() == pytest.approx(0.25)


def test_layout_order_and_qss_object_names(window):
    central_layout = window.workflow_shell.central_widget.layout()
    assert central_layout.itemAt(2).widget().objectName() == "currentStepCard"
    assert central_layout.itemAt(3).widget() is window.workflow_shell.workflow_stack
    setup_layout = window.workflow_shell.workflow_stack.widget(0).layout()
    setup_top = setup_layout.itemAt(0).layout()
    assert setup_top.itemAt(0).widget().objectName() == "sensorSystemGroup"
    assert setup_top.itemAt(1).widget().objectName() == "sensorMappingGroup"
    assert setup_layout.itemAt(1).widget().objectName() == "sessionDetailsGroup"

    names = {widget.objectName() for widget in window.findChildren(QPushButton)}
    names.update(widget.objectName() for widget in window.findChildren(QLabel))
    names.add(window.workflow_shell.central_widget.objectName())
    assert {"appRoot", "applicationTitle", "dangerButton"} <= names
    assert (
        window.workflow_shell.central_widget.findChild(QGroupBox, "currentStepCard")
        is not None
    )


def test_dimensions_and_important_size_policies(window):
    table = window.patient_ui.sensor_mapping_table
    assert table.minimumHeight() == 230
    assert table.maximumHeight() == 300
    assert window.recording_ui.recording_primary_button.minimumHeight() == 52
    assert window.status_ui.guidance_label.minimumHeight() == 50
    assert window.patient_ui.pose_box.sizePolicy().horizontalPolicy() == (
        QSizePolicy.Policy.Expanding
    )
    assert window.patient_ui.start_backend_button.sizePolicy().verticalPolicy() == (
        QSizePolicy.Policy.Fixed
    )


@pytest.mark.parametrize(
    ("phase", "enabled"),
    [
        ("stopped", {"start", "preview", "generate"}),
        ("sensors", {"preview", "generate"}),
        ("heading", {"heading", "preview", "generate"}),
        ("session", {"confirm", "preview", "generate"}),
        ("menu", {"static", "functional", "record", "exit", "preview", "generate"}),
        ("joint", {"preview", "generate"}),
        ("recording", {"record", "preview", "generate"}),
        ("busy", {"preview", "generate"}),
    ],
)
def test_enabled_actions_for_each_backend_phase(window, monkeypatch, phase, enabled):
    running = phase != "stopped"
    monkeypatch.setattr(window.processes, "backend_is_running", lambda: running)
    monkeypatch.setattr(window.processes, "viewer_is_active", lambda: False)
    monkeypatch.setattr(window.processes, "model_is_running", lambda: False)
    window.patient_ui.session_name_input.setText("P001")
    window.workflow.state.backend_state = phase
    window.workflow.state.connected_sensor_count = EXPECTED_SENSOR_COUNT
    window.workflow.state.model_prepared = True
    window.workflow.state.static_calibrated = True

    window.update_controls()

    actual = {
        "start": window.patient_ui.start_backend_button.isEnabled(),
        "heading": window.patient_ui.reset_headings_button.isEnabled(),
        "confirm": window.patient_ui.submit_session_button.isEnabled(),
        "static": window.calibration_ui.static_calibration_button.isEnabled(),
        "functional": window.calibration_ui.start_joint_button.isEnabled(),
        "record": window.recording_ui.recording_primary_button.isEnabled(),
        "exit": window.recording_ui.exit_backend_button.isEnabled(),
        "preview": window.recording_ui.start_viewer_button.isEnabled(),
        "generate": window.patient_ui.generate_model_button.isEnabled(),
    }
    assert {name for name, is_enabled in actual.items() if is_enabled} == enabled


def test_hidden_recording_buttons_do_not_exist(window):
    assert not hasattr(window.calibration_ui, "start_recording_button")
    assert not hasattr(window.calibration_ui, "stop_recording_button")


class FakeStyleHints:
    def __init__(self, scheme):
        self.scheme = scheme

    def colorScheme(self):
        return self.scheme


class FakeApplication:
    def __init__(self, scheme):
        self.hints = FakeStyleHints(scheme)
        self.properties = {}
        self.stylesheet = ""

    def styleHints(self):
        return self.hints

    def palette(self):
        return QApplication.instance().palette()

    def setProperty(self, name, value):
        self.properties[name] = value

    def setStyleSheet(self, stylesheet):
        self.stylesheet = stylesheet


@pytest.mark.parametrize("dark_theme", [False, True])
def test_light_and_dark_stylesheet_selection(tmp_path, dark_theme):
    from PySide6.QtCore import Qt

    (tmp_path / "style.qss").write_text("light-contract", encoding="utf-8")
    (tmp_path / "style_dark.qss").write_text("dark-contract", encoding="utf-8")
    scheme = Qt.ColorScheme.Dark if dark_theme else Qt.ColorScheme.Light
    application = FakeApplication(scheme)

    load_application_stylesheet(application, stylesheet_directory=tmp_path)

    assert application.properties["darkTheme"] is dark_theme
    assert application.stylesheet == (
        "dark-contract" if dark_theme else "light-contract"
    )
