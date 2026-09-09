from collections.abc import Callable
from dataclasses import dataclass

from PySide6.QtCore import Qt
from PySide6.QtWidgets import (
    QCheckBox,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QVBoxLayout,
    QWidget,
)


@dataclass(frozen=True, slots=True)
class RecordingPageHandlers:
    start_viewer: Callable[[], None]
    stop_viewer: Callable[[], None]
    toggle_recording: Callable[[], None]
    exit_sensor_system: Callable[[], None]


@dataclass(frozen=True, slots=True)
class RecordingPageUi:
    start_viewer_button: QPushButton
    stop_viewer_button: QPushButton
    recording_primary_button: QPushButton
    recording_state_label: QLabel
    recording_elapsed_label: QLabel
    recording_output_label: QLabel
    recording_viewer_status_label: QLabel
    stationary_feet_checkbox: QCheckBox
    exit_backend_button: QPushButton
    recording_group: QGroupBox
    page: QWidget


def build_recording_page(handlers: RecordingPageHandlers) -> RecordingPageUi:
    start_viewer_button = QPushButton("Preview calibrated pose")
    start_viewer_button.clicked.connect(handlers.start_viewer)
    stop_viewer_button = QPushButton("Stop preview")
    stop_viewer_button.setObjectName("dangerButton")
    stop_viewer_button.clicked.connect(handlers.stop_viewer)

    recording_primary_button = QPushButton("Start recording")
    recording_primary_button.setMinimumHeight(52)
    recording_primary_button.clicked.connect(handlers.toggle_recording)

    recording_state_label = QLabel("Ready to record")
    recording_state_label.setStyleSheet("font-size: 18px; font-weight: bold;")
    recording_elapsed_label = QLabel("00:00")
    recording_elapsed_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
    recording_elapsed_label.setStyleSheet("font-size: 28px; font-weight: bold;")
    recording_output_label = QLabel("No recording has been made in this session.")
    recording_output_label.setWordWrap(True)
    recording_viewer_status_label = QLabel("OpenSim viewer: stopped")
    recording_viewer_status_label.setStyleSheet("font-weight: bold; color: #6b7280;")

    stationary_feet_checkbox = QCheckBox(
        "Keep feet stationary and estimate pelvis translation"
    )
    stationary_feet_checkbox.setChecked(False)
    stationary_feet_checkbox.setToolTip(
        "Enable when the participant will keep both feet planted. "
        "This estimates whole-body translation for movements such as "
        "sit-to-stand."
    )

    exit_backend_button = QPushButton("Exit sensor system safely")
    exit_backend_button.setObjectName("dangerButton")
    exit_backend_button.clicked.connect(handlers.exit_sensor_system)

    recording_group = QGroupBox("Recording")
    recording_layout = QVBoxLayout()
    recording_status_layout = QHBoxLayout()
    recording_status_layout.addWidget(recording_state_label)
    recording_status_layout.addStretch()
    recording_status_layout.addWidget(recording_elapsed_label)
    recording_layout.addLayout(recording_status_layout)
    recording_layout.addWidget(recording_viewer_status_label)
    recording_options_layout = QHBoxLayout()
    recording_options_layout.addWidget(stationary_feet_checkbox)
    recording_options_layout.addStretch()
    recording_layout.addLayout(recording_options_layout)
    recording_layout.addWidget(recording_primary_button)
    recording_layout.addWidget(recording_output_label)
    recording_secondary_layout = QHBoxLayout()
    recording_secondary_layout.addStretch()
    recording_secondary_layout.addWidget(exit_backend_button)
    recording_layout.addLayout(recording_secondary_layout)
    recording_group.setLayout(recording_layout)

    page = QWidget()
    page_layout = QVBoxLayout(page)
    viewer_controls = QHBoxLayout()
    viewer_controls.addWidget(start_viewer_button)
    viewer_controls.addWidget(stop_viewer_button)
    viewer_controls.addStretch()
    page_layout.addLayout(viewer_controls)
    page_layout.addWidget(recording_group)
    page_layout.addStretch()

    return RecordingPageUi(
        start_viewer_button=start_viewer_button,
        stop_viewer_button=stop_viewer_button,
        recording_primary_button=recording_primary_button,
        recording_state_label=recording_state_label,
        recording_elapsed_label=recording_elapsed_label,
        recording_output_label=recording_output_label,
        recording_viewer_status_label=recording_viewer_status_label,
        stationary_feet_checkbox=stationary_feet_checkbox,
        exit_backend_button=exit_backend_button,
        recording_group=recording_group,
        page=page,
    )
