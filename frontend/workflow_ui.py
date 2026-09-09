from collections.abc import Callable
from dataclasses import dataclass

from PySide6.QtWidgets import (
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QPlainTextEdit,
    QPushButton,
    QStackedWidget,
    QVBoxLayout,
    QWidget,
)

from frontend.sensor_mapping import EXPECTED_SENSOR_COUNT


@dataclass(frozen=True, slots=True)
class DiagnosticsPageUi:
    status_label: QLabel
    guidance_label: QLabel
    compact_session_label: QLabel
    sensor_summary_label: QLabel
    static_summary_label: QLabel
    functional_summary_label: QLabel
    recording_summary_label: QLabel
    session_status_group: QGroupBox
    backend_log: QPlainTextEdit
    viewer_log: QPlainTextEdit
    page: QWidget


@dataclass(frozen=True, slots=True)
class WorkflowContent:
    setup_page: QWidget
    calibration_page: QWidget
    recording_page: QWidget
    diagnostics_page: QWidget
    status_label: QLabel
    guidance_label: QLabel
    compact_session_label: QLabel


@dataclass(frozen=True, slots=True)
class WorkflowShell:
    central_widget: QWidget
    header_session_label: QLabel
    header_sensor_label: QLabel
    header_backend_label: QLabel
    header_viewer_label: QLabel
    workflow_stack: QStackedWidget
    workflow_buttons: tuple[QPushButton, ...]


def build_diagnostics_page() -> DiagnosticsPageUi:
    status_label = QLabel("Status: Sensor system not started")
    status_label.setStyleSheet("padding: 8px; font-weight: bold;")

    guidance_label = QLabel(
        "Guidance: Start the sensor system, enter the session details, "
        "generate the patient model, and confirm the session."
    )
    guidance_label.setWordWrap(True)
    guidance_label.setMinimumHeight(50)
    guidance_label.setStyleSheet(
        "padding: 10px;"
        "font-size: 14px;"
        "font-weight: bold;"
        "color: #164f73;"
        "background-color: #e2f2fb;"
        "border: 1px solid #8fc5e3;"
    )

    compact_session_label = QLabel(
        "Model not ready  •  "
        f"Sensors 0/{EXPECTED_SENSOR_COUNT}  •  "
        "Static pending  •  Recording stopped"
    )
    compact_session_label.setWordWrap(True)
    compact_session_label.setStyleSheet("padding: 6px 10px;")

    session_status_group = QGroupBox("Current session status")
    session_status_layout = QVBoxLayout()
    sensor_summary_label = QLabel("Sensors: not checked")
    static_summary_label = QLabel("Static calibration: not completed")
    functional_summary_label = QLabel("Functional calibrations: none")
    recording_summary_label = QLabel("Recording: stopped")
    session_status_layout.addWidget(sensor_summary_label)
    session_status_layout.addWidget(static_summary_label)
    session_status_layout.addWidget(functional_summary_label)
    session_status_layout.addWidget(recording_summary_label)
    session_status_group.setLayout(session_status_layout)

    backend_log = QPlainTextEdit()
    backend_log.setReadOnly(True)
    viewer_log = QPlainTextEdit()
    viewer_log.setReadOnly(True)

    diagnostics_page = QWidget()
    diagnostics_layout = QVBoxLayout(diagnostics_page)
    diagnostics_layout.addWidget(session_status_group)
    diagnostics_layout.addWidget(QLabel("Sensor system output"))
    diagnostics_layout.addWidget(backend_log)
    diagnostics_layout.addWidget(QLabel("OpenSim output"))
    diagnostics_layout.addWidget(viewer_log)

    return DiagnosticsPageUi(
        status_label=status_label,
        guidance_label=guidance_label,
        compact_session_label=compact_session_label,
        sensor_summary_label=sensor_summary_label,
        static_summary_label=static_summary_label,
        functional_summary_label=functional_summary_label,
        recording_summary_label=recording_summary_label,
        session_status_group=session_status_group,
        backend_log=backend_log,
        viewer_log=viewer_log,
        page=diagnostics_page,
    )


def build_workflow_shell(
    content: WorkflowContent,
    on_page_selected: Callable[[int], None],
) -> WorkflowShell:
    header_layout = QHBoxLayout()
    header_title = QLabel("IRP Seated Motion Capture")
    header_title.setObjectName("applicationTitle")
    header_title.setStyleSheet("font-size: 18px; font-weight: bold;")

    header_session_label = QLabel("Session: not set")
    header_sensor_label = QLabel(f"Sensors: 0/{EXPECTED_SENSOR_COUNT}")
    header_backend_label = QLabel("Sensor system: stopped")
    header_viewer_label = QLabel("Viewer: stopped")

    for header_chip in (
        header_session_label,
        header_sensor_label,
        header_backend_label,
        header_viewer_label,
    ):
        header_chip.setProperty("headerChip", True)

    header_layout.addWidget(header_title)
    header_layout.addStretch()
    header_layout.addWidget(header_session_label)
    header_layout.addWidget(header_sensor_label)
    header_layout.addWidget(header_backend_label)
    header_layout.addWidget(header_viewer_label)

    workflow_stack = QStackedWidget()
    workflow_buttons = []
    navigation_layout = QHBoxLayout()
    workflow_names = (
        "1  Setup",
        "2  Calibration",
        "3  Recording",
        "Diagnostics",
    )

    for index, name in enumerate(workflow_names):
        button = QPushButton(name)
        button.setProperty("workflowStep", True)
        button.setCheckable(True)
        button.clicked.connect(
            lambda _checked=False, page=index: on_page_selected(page)
        )
        navigation_layout.addWidget(button)
        workflow_buttons.append(button)

    for page in (
        content.setup_page,
        content.calibration_page,
        content.recording_page,
        content.diagnostics_page,
    ):
        workflow_stack.addWidget(page)

    message_group = QGroupBox("Current step")
    message_group.setObjectName("currentStepCard")
    message_layout = QVBoxLayout()
    message_layout.addWidget(content.status_label)
    message_layout.addWidget(content.guidance_label)
    message_layout.addWidget(content.compact_session_label)
    message_group.setLayout(message_layout)

    layout = QVBoxLayout()
    layout.addLayout(header_layout)
    layout.addLayout(navigation_layout)
    layout.addWidget(message_group)
    layout.addWidget(workflow_stack, 1)

    central_widget = QWidget()
    central_widget.setObjectName("appRoot")
    central_widget.setLayout(layout)

    return WorkflowShell(
        central_widget=central_widget,
        header_session_label=header_session_label,
        header_sensor_label=header_sensor_label,
        header_backend_label=header_backend_label,
        header_viewer_label=header_viewer_label,
        workflow_stack=workflow_stack,
        workflow_buttons=tuple(workflow_buttons),
    )
