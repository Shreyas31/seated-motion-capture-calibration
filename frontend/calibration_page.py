from collections.abc import Callable
from dataclasses import dataclass

from PySide6.QtWidgets import (
    QComboBox,
    QGridLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QVBoxLayout,
    QWidget,
)

from frontend.protocol import FUNCTIONAL_CALIBRATIONS


@dataclass(frozen=True, slots=True)
class CalibrationPageHandlers:
    start_static_calibration: Callable[[], None]
    start_functional_calibration: Callable[[], None]


@dataclass(frozen=True, slots=True)
class CalibrationPageUi:
    static_calibration_button: QPushButton
    joint_box: QComboBox
    start_joint_button: QPushButton
    static_calibration_status_label: QLabel
    functional_status_labels: dict[str, QLabel]
    calibration_group: QGroupBox
    page: QWidget


def build_calibration_page(handlers: CalibrationPageHandlers) -> CalibrationPageUi:
    static_calibration_button = QPushButton("Perform static calibration")
    static_calibration_button.clicked.connect(handlers.start_static_calibration)

    joint_box = QComboBox()
    for movement_name, backend_choice in FUNCTIONAL_CALIBRATIONS:
        joint_box.addItem(movement_name, backend_choice)

    start_joint_button = QPushButton("Start selected functional calibration")
    start_joint_button.clicked.connect(handlers.start_functional_calibration)

    calibration_group = QGroupBox("Calibration workflow")
    calibration_layout = QVBoxLayout()
    static_group = QGroupBox("Required static calibration")
    static_layout = QHBoxLayout()
    static_calibration_status_label = QLabel("Not performed")
    static_calibration_status_label.setStyleSheet("color: #8a6500; font-weight: bold;")
    static_layout.addWidget(static_calibration_status_label)
    static_layout.addStretch()
    static_layout.addWidget(static_calibration_button)
    static_group.setLayout(static_layout)
    calibration_layout.addWidget(static_group)

    functional_group = QGroupBox("Optional functional calibrations")
    functional_layout = QVBoxLayout()
    functional_explanation = QLabel(
        "Use functional movements when the participant can perform them "
        "safely. Failed movements may be repeated."
    )
    functional_explanation.setWordWrap(True)
    functional_layout.addWidget(functional_explanation)
    functional_status_labels = {}
    functional_status_grid = QGridLayout()

    for row, (movement_name, _choice) in enumerate(FUNCTIONAL_CALIBRATIONS):
        name_label = QLabel(movement_name)
        status_label = QLabel("Not performed")
        status_label.setStyleSheet("color: #6b7280; font-weight: bold;")
        functional_status_grid.addWidget(name_label, row, 0)
        functional_status_grid.addWidget(status_label, row, 1)
        functional_status_labels[movement_name] = status_label

    functional_status_grid.setColumnStretch(0, 1)
    functional_layout.addLayout(functional_status_grid)
    functional_selection_layout = QHBoxLayout()
    functional_selection_layout.addWidget(QLabel("Movement:"))
    functional_selection_layout.addWidget(joint_box, 1)
    functional_selection_layout.addWidget(start_joint_button)
    functional_layout.addLayout(functional_selection_layout)
    functional_group.setLayout(functional_layout)
    calibration_layout.addWidget(functional_group)
    calibration_group.setLayout(calibration_layout)

    page = QWidget()
    page_layout = QVBoxLayout(page)
    page_layout.addWidget(calibration_group)
    page_layout.addStretch()

    return CalibrationPageUi(
        static_calibration_button=static_calibration_button,
        joint_box=joint_box,
        start_joint_button=start_joint_button,
        static_calibration_status_label=static_calibration_status_label,
        functional_status_labels=functional_status_labels,
        calibration_group=calibration_group,
        page=page,
    )
