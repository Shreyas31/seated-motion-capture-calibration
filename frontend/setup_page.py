from collections.abc import Callable, Mapping
from dataclasses import dataclass
from pathlib import Path

from PySide6.QtCore import Qt
from PySide6.QtGui import QColor
from PySide6.QtWidgets import (
    QAbstractItemView,
    QAbstractSpinBox,
    QComboBox,
    QDialog,
    QDialogButtonBox,
    QDoubleSpinBox,
    QFormLayout,
    QGroupBox,
    QHBoxLayout,
    QHeaderView,
    QLabel,
    QLineEdit,
    QMessageBox,
    QProgressBar,
    QPushButton,
    QSizePolicy,
    QTableWidget,
    QTableWidgetItem,
    QVBoxLayout,
    QWidget,
)

from frontend.sensor_mapping import (
    CANONICAL_SEGMENTS,
    EXPECTED_SENSOR_COUNT,
    save_sensor_mapping,
)


@dataclass(frozen=True, slots=True)
class SetupPageHandlers:
    update_controls: Callable[..., None]
    start_sensor_system: Callable[[], None]
    run_system_check: Callable[[], None]
    reload_sensor_mapping: Callable[[], None]
    configure_sensor_mapping: Callable[[], None]
    toggle_sensor_table: Callable[[bool], None]
    reset_heading_offsets: Callable[[], None]
    submit_session: Callable[[], None]
    generate_model: Callable[[], None]
    patient_inputs_changed: Callable[..., None]


@dataclass(frozen=True, slots=True)
class SetupPageUi:
    pose_box: QComboBox
    start_backend_button: QPushButton
    system_check_button: QPushButton
    reload_sensor_mapping_button: QPushButton
    configure_sensor_mapping_button: QPushButton
    sensor_mapping_status_label: QLabel
    sensor_connection_overview_label: QLabel
    sensor_connection_progress: QProgressBar
    sensor_problem_label: QLabel
    toggle_sensor_table_button: QPushButton
    sensor_mapping_table: QTableWidget
    reset_headings_button: QPushButton
    session_name_input: QLineEdit
    submit_session_button: QPushButton
    height_input: QDoubleSpinBox
    foot_length_input: QDoubleSpinBox
    generate_model_button: QPushButton
    model_summary_label: QLabel
    patient_group: QGroupBox
    sensor_mapping_group: QGroupBox
    page: QWidget


def build_setup_page(
    handlers: SetupPageHandlers,
    segments: tuple[str, ...],
) -> SetupPageUi:
    pose_box = QComboBox()
    pose_box.addItem("Chair seated", "chair")
    pose_box.addItem("Bed seated", "bed")
    pose_box.setSizePolicy(
        QSizePolicy.Policy.Expanding,
        QSizePolicy.Policy.Fixed,
    )
    pose_box.setSizeAdjustPolicy(
        QComboBox.SizeAdjustPolicy.AdjustToMinimumContentsLengthWithIcon
    )
    pose_box.setMinimumContentsLength(12)

    pose_box.currentIndexChanged.connect(handlers.update_controls)
    pose_box.currentIndexChanged.connect(handlers.patient_inputs_changed)

    start_backend_button = QPushButton("Start sensor system")
    start_backend_button.clicked.connect(handlers.start_sensor_system)
    system_check_button = QPushButton("Run system check")
    system_check_button.clicked.connect(handlers.run_system_check)

    reload_sensor_mapping_button = QPushButton("Reload sensor mapping")
    reload_sensor_mapping_button.clicked.connect(handlers.reload_sensor_mapping)
    configure_sensor_mapping_button = QPushButton("Configure sensor mapping")
    configure_sensor_mapping_button.clicked.connect(handlers.configure_sensor_mapping)

    sensor_mapping_status_label = QLabel("Sensor mapping: not loaded")
    sensor_connection_overview_label = QLabel(
        f"0 of {EXPECTED_SENSOR_COUNT} sensors connected"
    )
    sensor_connection_overview_label.setStyleSheet(
        "font-size: 18px; font-weight: bold;"
    )
    sensor_connection_progress = QProgressBar()
    sensor_connection_progress.setRange(0, len(segments))
    sensor_connection_progress.setValue(0)
    sensor_connection_progress.setTextVisible(False)
    sensor_problem_label = QLabel("Waiting for sensor connection information.")
    sensor_problem_label.setWordWrap(True)

    toggle_sensor_table_button = QPushButton("View all sensors")
    toggle_sensor_table_button.setCheckable(True)
    toggle_sensor_table_button.toggled.connect(handlers.toggle_sensor_table)

    sensor_mapping_table = QTableWidget()
    sensor_mapping_table.setColumnCount(3)
    sensor_mapping_table.setHorizontalHeaderLabels(["Segment", "Sensor ID", "Status"])
    sensor_mapping_table.setRowCount(len(segments))
    sensor_mapping_table.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
    sensor_mapping_table.setSelectionBehavior(
        QAbstractItemView.SelectionBehavior.SelectRows
    )
    sensor_mapping_table.setAlternatingRowColors(True)
    sensor_mapping_table.setMinimumHeight(230)
    sensor_mapping_table.setMaximumHeight(300)
    mapping_header = sensor_mapping_table.horizontalHeader()
    mapping_header.setSectionResizeMode(0, QHeaderView.ResizeMode.Stretch)
    mapping_header.setSectionResizeMode(1, QHeaderView.ResizeMode.ResizeToContents)
    mapping_header.setSectionResizeMode(2, QHeaderView.ResizeMode.ResizeToContents)
    sensor_mapping_table.setVisible(False)

    reset_headings_button = QPushButton("Reset heading offsets")
    reset_headings_button.clicked.connect(handlers.reset_heading_offsets)

    session_name_input = QLineEdit()
    session_name_input.setPlaceholderText("For example P01_20260805")
    session_name_input.setSizePolicy(
        QSizePolicy.Policy.Expanding,
        QSizePolicy.Policy.Fixed,
    )
    submit_session_button = QPushButton("Confirm session")
    submit_session_button.clicked.connect(handlers.submit_session)

    height_input = QDoubleSpinBox()
    height_input.setRange(1.0, 2.3)
    height_input.setDecimals(3)
    height_input.setSingleStep(0.01)
    height_input.setValue(1.70)
    height_input.setSuffix(" m")
    height_input.setButtonSymbols(QAbstractSpinBox.ButtonSymbols.NoButtons)
    height_input.setSizePolicy(
        QSizePolicy.Policy.Expanding,
        QSizePolicy.Policy.Fixed,
    )

    foot_length_input = QDoubleSpinBox()
    foot_length_input.setRange(0.15, 0.40)
    foot_length_input.setDecimals(3)
    foot_length_input.setSingleStep(0.005)
    foot_length_input.setValue(0.25)
    foot_length_input.setSuffix(" m")
    foot_length_input.setButtonSymbols(QAbstractSpinBox.ButtonSymbols.NoButtons)
    foot_length_input.setSizePolicy(
        QSizePolicy.Policy.Expanding,
        QSizePolicy.Policy.Fixed,
    )

    generate_model_button = QPushButton("Generate patient OpenSim model")
    generate_model_button.clicked.connect(handlers.generate_model)
    model_summary_label = QLabel("Patient model: not generated")
    model_summary_label.setStyleSheet("color: #8b1a1a;")
    model_summary_label.setWordWrap(False)

    session_name_input.textChanged.connect(handlers.patient_inputs_changed)
    height_input.valueChanged.connect(handlers.patient_inputs_changed)
    foot_length_input.valueChanged.connect(handlers.patient_inputs_changed)

    patient_group = QGroupBox("Session details")
    patient_layout = QFormLayout()
    patient_layout.setFieldGrowthPolicy(
        QFormLayout.FieldGrowthPolicy.AllNonFixedFieldsGrow
    )

    patient_layout.addRow(
        "Calibration pose:",
        pose_box,
    )

    patient_layout.addRow(
        "Patient height:",
        height_input,
    )

    patient_layout.addRow(
        "Foot length:",
        foot_length_input,
    )

    patient_layout.addRow(
        "Session name:",
        session_name_input,
    )

    # Preserve the original status-row spacing so the action buttons remain
    # at their previous vertical position.
    model_status_spacer = QLabel("")
    model_status_spacer.setFixedHeight(model_summary_label.sizeHint().height())
    patient_layout.addRow(model_status_spacer)

    # Align model status with both actions on the final row.
    session_action_layout = QHBoxLayout()
    session_action_layout.addWidget(model_summary_label)
    session_action_layout.addStretch(1)
    session_action_layout.addWidget(generate_model_button)
    session_action_layout.addWidget(submit_session_button)

    patient_layout.addRow(session_action_layout)

    patient_group.setLayout(patient_layout)

    sensor_mapping_group = QGroupBox("Sensor-to-segment mapping")
    sensor_mapping_layout = QVBoxLayout()
    sensor_overview_layout = QHBoxLayout()
    sensor_overview_layout.addWidget(sensor_connection_overview_label)
    sensor_overview_layout.addWidget(sensor_connection_progress, 1)
    sensor_mapping_layout.addLayout(sensor_overview_layout)
    sensor_mapping_layout.addWidget(sensor_problem_label)
    sensor_mapping_controls = QHBoxLayout()
    sensor_mapping_controls.addWidget(sensor_mapping_status_label)
    sensor_mapping_controls.addStretch()
    sensor_mapping_controls.addWidget(toggle_sensor_table_button)
    sensor_mapping_controls.addWidget(reload_sensor_mapping_button)
    sensor_mapping_controls.addWidget(configure_sensor_mapping_button)
    sensor_mapping_layout.addLayout(sensor_mapping_controls)
    sensor_mapping_layout.addWidget(sensor_mapping_table)
    sensor_mapping_group.setLayout(sensor_mapping_layout)

    setup_page = QWidget()
    setup_page_layout = QVBoxLayout(setup_page)

    setup_top_layout = QHBoxLayout()
    setup_top_layout.setSpacing(12)

    system_start_group = QGroupBox("Sensor system")
    system_start_group.setObjectName("sensorSystemGroup")
    system_start_group.setSizePolicy(
        QSizePolicy.Policy.Maximum,
        QSizePolicy.Policy.Preferred,
    )

    system_start_layout = QVBoxLayout()
    for button in (
        start_backend_button,
        system_check_button,
        reset_headings_button,
    ):
        button.setSizePolicy(
            QSizePolicy.Policy.Expanding,
            QSizePolicy.Policy.Fixed,
        )
        system_start_layout.addWidget(button)
    system_start_group.setLayout(system_start_layout)
    setup_top_layout.addWidget(system_start_group, 0)

    sensor_mapping_group.setObjectName("sensorMappingGroup")
    sensor_mapping_group.setSizePolicy(
        QSizePolicy.Policy.Expanding,
        QSizePolicy.Policy.Preferred,
    )
    setup_top_layout.addWidget(sensor_mapping_group, 1)
    setup_page_layout.addLayout(setup_top_layout)

    patient_group.setObjectName("sessionDetailsGroup")
    patient_group.setSizePolicy(
        QSizePolicy.Policy.Expanding,
        QSizePolicy.Policy.Preferred,
    )
    setup_page_layout.addWidget(patient_group)
    setup_page_layout.addStretch()

    return SetupPageUi(
        pose_box=pose_box,
        start_backend_button=start_backend_button,
        system_check_button=system_check_button,
        reload_sensor_mapping_button=reload_sensor_mapping_button,
        configure_sensor_mapping_button=configure_sensor_mapping_button,
        sensor_mapping_status_label=sensor_mapping_status_label,
        sensor_connection_overview_label=sensor_connection_overview_label,
        sensor_connection_progress=sensor_connection_progress,
        sensor_problem_label=sensor_problem_label,
        toggle_sensor_table_button=toggle_sensor_table_button,
        sensor_mapping_table=sensor_mapping_table,
        reset_headings_button=reset_headings_button,
        session_name_input=session_name_input,
        submit_session_button=submit_session_button,
        height_input=height_input,
        foot_length_input=foot_length_input,
        generate_model_button=generate_model_button,
        model_summary_label=model_summary_label,
        patient_group=patient_group,
        sensor_mapping_group=sensor_mapping_group,
        page=setup_page,
    )


class SensorMappingDialog(QDialog):
    def __init__(
        self,
        mapping: Mapping[str, str],
        mapping_path: Path,
        parent: QWidget | None = None,
    ) -> None:
        super().__init__(parent)

        self.mapping_path = Path(mapping_path)
        self.setWindowTitle("Configure sensor-to-segment mapping")
        self.resize(650, 600)

        explanation = QLabel(
            "Assign one physical Xsens sensor ID to each anatomical "
            "segment. Segment names are fixed because calibration and "
            "OpenSim depend on them."
        )
        explanation.setWordWrap(True)

        path_label = QLabel(f"Configuration file: {self.mapping_path}")
        path_label.setWordWrap(True)
        path_label.setStyleSheet("color: #555555;")

        self.table = QTableWidget()
        self.table.setColumnCount(2)
        self.table.setRowCount(len(CANONICAL_SEGMENTS))
        self.table.setHorizontalHeaderLabels(["Segment", "Sensor ID"])
        self.table.setAlternatingRowColors(True)

        header = self.table.horizontalHeader()
        header.setSectionResizeMode(0, QHeaderView.ResizeMode.Stretch)
        header.setSectionResizeMode(1, QHeaderView.ResizeMode.Stretch)

        for row, segment in enumerate(CANONICAL_SEGMENTS):
            segment_item = QTableWidgetItem(segment)
            segment_item.setFlags(segment_item.flags() & ~Qt.ItemFlag.ItemIsEditable)
            sensor_item = QTableWidgetItem(mapping.get(segment, ""))
            self.table.setItem(row, 0, segment_item)
            self.table.setItem(row, 1, sensor_item)

        self.validation_label = QLabel()
        self.validation_label.setWordWrap(True)

        self.buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Save
            | QDialogButtonBox.StandardButton.Cancel
        )
        self.save_button = self.buttons.button(QDialogButtonBox.StandardButton.Save)

        self.buttons.rejected.connect(self.reject)
        self.save_button.clicked.connect(self.save_and_accept)
        self.table.itemChanged.connect(self.validate_table)

        layout = QVBoxLayout()
        layout.addWidget(explanation)
        layout.addWidget(path_label)
        layout.addWidget(self.table)
        layout.addWidget(self.validation_label)
        layout.addWidget(self.buttons)
        self.setLayout(layout)

        self.validate_table()

    def table_mapping(self) -> dict[str, str]:
        mapping = {}

        for row, segment in enumerate(CANONICAL_SEGMENTS):
            sensor_item = self.table.item(row, 1)
            sensor_id = (
                sensor_item.text().strip().upper() if sensor_item is not None else ""
            )
            mapping[segment] = sensor_id

        return mapping

    def validate_table(self, *_args) -> bool:
        mapping = self.table_mapping()
        missing_segments = [
            segment for segment, sensor_id in mapping.items() if not sensor_id
        ]
        sensor_counts: dict[str, int] = {}

        for sensor_id in mapping.values():
            if sensor_id:
                sensor_counts[sensor_id] = sensor_counts.get(sensor_id, 0) + 1

        duplicate_ids = {
            sensor_id for sensor_id, count in sensor_counts.items() if count > 1
        }

        self.table.blockSignals(True)
        normal_colour = QColor("#ffffff")
        error_colour = QColor("#f8dddd")
        sensor_text_colour = QColor("#000000")

        for row, _segment in enumerate(CANONICAL_SEGMENTS):
            sensor_item = self.table.item(row, 1)

            if sensor_item is None:
                continue

            sensor_id = sensor_item.text().strip().upper()
            is_invalid = not sensor_id or sensor_id in duplicate_ids
            sensor_item.setBackground(error_colour if is_invalid else normal_colour)
            sensor_item.setForeground(sensor_text_colour)

        self.table.blockSignals(False)
        problems = []

        if missing_segments:
            problems.append("Missing IDs: " + ", ".join(missing_segments))

        if duplicate_ids:
            problems.append("Duplicate IDs: " + ", ".join(sorted(duplicate_ids)))

        if problems:
            self.validation_label.setText("Invalid mapping — " + " | ".join(problems))
            self.validation_label.setStyleSheet("color: #8b1a1a; font-weight: bold;")
            self.save_button.setEnabled(False)
            return False

        self.validation_label.setText(
            f"Valid mapping: {len(CANONICAL_SEGMENTS)} unique sensor IDs assigned."
        )
        self.validation_label.setStyleSheet("color: #176b2c; font-weight: bold;")
        self.save_button.setEnabled(True)
        return True

    def save_and_accept(self) -> None:
        if not self.validate_table():
            return

        try:
            save_sensor_mapping(self.mapping_path, self.table_mapping())
        except ValueError as error:
            QMessageBox.critical(
                self,
                "Could not save mapping",
                str(error),
            )
            return

        self.accept()
