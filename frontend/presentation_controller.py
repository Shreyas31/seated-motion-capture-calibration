"""Workflow presentation and sensor-mapping behavior for the main window."""

from PySide6.QtCore import Qt
from PySide6.QtGui import QColor
from PySide6.QtWidgets import QDialog, QMessageBox, QTableWidgetItem

from frontend.runtime_context import APP_PATHS, LOGGER, SENSOR_MAPPING_FILE
from frontend.sensor_mapping import (
    CANONICAL_SEGMENTS,
    EXPECTED_SENSOR_COUNT,
    load_sensor_mapping,
)
from frontend.setup_page import SensorMappingDialog
from frontend.system_check import run_preflight_checks
from frontend.workflow_ui import WorkflowContent, build_workflow_shell


class PresentationMixin:
    """Maintains workflow navigation, summaries, and sensor-mapping presentation."""

    def build_main_layout(self):
        """Assemble the clinician-facing workflow pages."""
        content = WorkflowContent(
            setup_page=self.patient_ui.page,
            calibration_page=self.calibration_ui.page,
            recording_page=self.recording_ui.page,
            diagnostics_page=self.status_ui.page,
            status_label=self.status_ui.status_label,
            guidance_label=self.status_ui.guidance_label,
            compact_session_label=self.status_ui.compact_session_label,
        )
        shell = build_workflow_shell(
            content,
            self.show_workflow_page,
        )
        self.workflow_shell = shell
        self.setCentralWidget(shell.central_widget)
        self.show_workflow_page(0)
        self.update_compact_session_summary()

    def show_workflow_page(self, index):
        self.workflow_shell.workflow_stack.setCurrentIndex(index)
        for button_index, button in enumerate(self.workflow_shell.workflow_buttons):
            button.setChecked(button_index == index)

        self.update_workflow_navigation()

    def guide_to_backend_state(self, backend_state):
        """Show the page containing the action requested by backend."""
        if backend_state == self.workflow.state.last_guided_backend_state:
            return

        page_for_state = {
            "sensors": 0,
            "heading": 0,
            "session": 0,
            "menu": 1,
            "joint": 1,
            "recording": 2,
        }
        page_index = page_for_state.get(backend_state)
        if page_index is not None:
            self.show_workflow_page(page_index)

        self.workflow.state.last_guided_backend_state = backend_state

    def update_workflow_navigation(self):
        if not hasattr(self, "workflow_shell"):
            return

        backend_running = self.processes.backend_is_active()
        patient_complete = (
            bool(self.patient_ui.session_name_input.text().strip())
            and self.workflow.state.model_prepared
        )
        sensors_complete = (
            backend_running
            and self.workflow.state.connected_sensor_count == EXPECTED_SENSOR_COUNT
        )
        calibration_complete = self.workflow.state.static_calibrated
        setup_complete = patient_complete and sensors_complete

        completed_steps = (
            setup_complete,
            calibration_complete,
            False,
            False,
        )

        default_names = (
            "1  Setup",
            "2  Calibration",
            "3  Recording",
            "Diagnostics",
        )

        for index, button in enumerate(self.workflow_shell.workflow_buttons):
            # Clinicians may review any page at any time. The action handlers
            # enforce prerequisites before sending commands to the backend.
            button.setEnabled(True)
            suffix = "  ✓" if completed_steps[index] else ""
            button.setText(default_names[index] + suffix)
            button.setProperty(
                "workflowComplete",
                completed_steps[index],
            )
            button.style().unpolish(button)
            button.style().polish(button)

    def update_workflow_header(self):
        if not hasattr(self, "workflow_shell"):
            return

        session_name = (
            self.workflow.state.confirmed_session_name
            or self.patient_ui.session_name_input.text().strip()
        )
        self.workflow_shell.header_session_label.setText(
            f"Session: {session_name or 'not set'}"
        )
        self.workflow_shell.header_sensor_label.setText(
            f"Sensors: {self.workflow.state.connected_sensor_count}/"
            f"{EXPECTED_SENSOR_COUNT}"
        )
        backend_running = self.processes.backend_is_active()
        viewer_running = self.processes.viewer_is_active()
        self.workflow_shell.header_backend_label.setText(
            "Sensor system: running" if backend_running else "Sensor system: stopped"
        )
        self.workflow_shell.header_viewer_label.setText(
            "Viewer: running" if viewer_running else "Viewer: stopped"
        )

    def update_compact_session_summary(self):
        if not hasattr(self, "status_ui"):
            return

        model_text = (
            "Model ready" if self.workflow.state.model_prepared else "Model not ready"
        )
        sensor_text = (
            f"Sensors {self.workflow.state.connected_sensor_count}/"
            f"{len(CANONICAL_SEGMENTS)}"
        )
        static_text = (
            "Static passed"
            if self.workflow.state.static_calibrated
            else "Static pending"
        )
        passed_count = len(self.workflow.state.functional_calibrations)
        failed_count = len(self.workflow.state.failed_functional_calibrations)
        functional_text = f"Functional {passed_count} passed"
        if failed_count:
            functional_text += f", {failed_count} failed"
        recording_text = (
            "Recording active"
            if self.workflow.state.recording_active
            else "Recording stopped"
        )

        self.status_ui.compact_session_label.setText(
            "  •  ".join(
                (
                    model_text,
                    sensor_text,
                    static_text,
                    functional_text,
                    recording_text,
                )
            )
        )

    def toggle_sensor_table(self, visible):
        self.patient_ui.sensor_mapping_table.setVisible(visible)
        self.patient_ui.toggle_sensor_table_button.setText(
            "Hide sensor details" if visible else "View all sensors"
        )

    def update_sensor_overview(self):
        if not hasattr(self, "patient_ui"):
            return

        detailed_connected_count = sum(
            1
            for connected in self.workflow.state.sensor_connection_status.values()
            if connected
        )
        connected_count = (
            detailed_connected_count
            if self.workflow.state.sensor_status_events_received
            else self.workflow.state.connected_sensor_count
        )
        expected_count = len(CANONICAL_SEGMENTS)
        self.patient_ui.sensor_connection_progress.setValue(connected_count)
        self.patient_ui.sensor_connection_overview_label.setText(
            f"{connected_count} of {expected_count} sensors connected"
        )

        disconnected_segments = [
            segment.replace("_", " ")
            for segment in CANONICAL_SEGMENTS
            if not self.workflow.state.sensor_connection_status.get(segment, False)
        ]

        if connected_count == expected_count:
            problem_text = "All required sensors are connected."
            overview_colour = "#176b2c"
        elif connected_count == 0:
            problem_text = (
                "No mapped sensors are connected yet. Start the "
                "sensor system and check the sensor connections."
            )
            overview_colour = "#6b7280"
        elif self.workflow.state.sensor_status_events_received:
            displayed = disconnected_segments[:6]
            remaining = len(disconnected_segments) - len(displayed)
            problem_text = "Disconnected: " + ", ".join(displayed)
            if remaining > 0:
                problem_text += f" and {remaining} more"
            overview_colour = "#8b1a1a"
        else:
            problem_text = (
                f"The sensor system reports {connected_count} connected "
                "sensors; detailed segment status is not available yet."
            )
            overview_colour = "#8a6500"

        if self.workflow.state.unmapped_connected_sensors:
            problem_text += (
                " Unassigned sensor IDs: "
                + ", ".join(sorted(self.workflow.state.unmapped_connected_sensors))
                + "."
            )
            overview_colour = "#8a6500"

        self.patient_ui.sensor_connection_overview_label.setStyleSheet(
            f"font-size: 18px; font-weight: bold; color: {overview_colour};"
        )
        self.patient_ui.sensor_problem_label.setText(problem_text)
        self.patient_ui.sensor_problem_label.setStyleSheet(
            f"color: {overview_colour}; padding: 4px 0;"
        )

    @staticmethod
    def normalise_calibration_name(name):
        return " ".join(name.replace("_", " ").lower().split())

    def update_calibration_cards(self):
        if not hasattr(self, "calibration_ui"):
            return

        if self.workflow.state.static_calibrated:
            pose_name = (
                self.workflow.state.static_pose.replace("_", " ").title()
                if self.workflow.state.static_pose
                else "Completed"
            )
            self.calibration_ui.static_calibration_status_label.setText(
                f"Passed — {pose_name}"
            )
            self.calibration_ui.static_calibration_status_label.setStyleSheet(
                "color: #176b2c; font-weight: bold;"
            )
        else:
            self.calibration_ui.static_calibration_status_label.setText("Not performed")
            self.calibration_ui.static_calibration_status_label.setStyleSheet(
                "color: #8a6500; font-weight: bold;"
            )

        passed_names = {
            self.normalise_calibration_name(name)
            for name in self.workflow.state.functional_calibrations
        }
        failed_by_name = {
            self.normalise_calibration_name(name): reason
            for name, reason in self.workflow.state.failed_functional_calibrations.items()
        }

        for (
            movement_name,
            status_label,
        ) in self.calibration_ui.functional_status_labels.items():
            normalised_name = self.normalise_calibration_name(movement_name)
            if normalised_name in passed_names:
                status_label.setText("Passed")
                status_label.setToolTip("")
                status_label.setStyleSheet("color: #176b2c; font-weight: bold;")
            elif normalised_name in failed_by_name:
                reason = failed_by_name[normalised_name].strip()
                status_label.setText(f"Failed — {reason}" if reason else "Failed")
                status_label.setToolTip(reason)
                status_label.setStyleSheet("color: #8b1a1a; font-weight: bold;")
            else:
                status_label.setText("Not performed")
                status_label.setToolTip("")
                status_label.setStyleSheet("color: #6b7280; font-weight: bold;")

    def toggle_recording_from_primary(self):
        if (
            self.workflow.state.recording_active
            or self.workflow.state.backend_state == "recording"
        ):
            self.stop_recording()
        else:
            self.start_recording()

    def update_recording_elapsed(self):
        if not self.recording_elapsed_timer.isValid():
            return

        total_seconds = max(
            0,
            self.recording_elapsed_timer.elapsed() // 1000,
        )
        minutes, seconds = divmod(total_seconds, 60)
        self.recording_ui.recording_elapsed_label.setText(
            f"{minutes:02d}:{seconds:02d}"
        )

    def update_recording_page(self):
        if not hasattr(self, "calibration_ui"):
            return

        viewer_running = self.processes.viewer_is_active()
        if viewer_running:
            self.recording_ui.recording_viewer_status_label.setText(
                "OpenSim viewer: running"
            )
            self.recording_ui.recording_viewer_status_label.setStyleSheet(
                "font-weight: bold; color: #176b2c;"
            )
        else:
            self.recording_ui.recording_viewer_status_label.setText(
                "OpenSim viewer: stopped"
            )
            self.recording_ui.recording_viewer_status_label.setStyleSheet(
                "font-weight: bold; color: #6b7280;"
            )

        recording_now = (
            self.workflow.state.recording_active
            or self.workflow.state.backend_state == "recording"
        )
        if recording_now:
            self.recording_ui.recording_primary_button.setText("Stop recording")
            self.recording_ui.recording_primary_button.setStyleSheet(
                "font-size: 16px; font-weight: bold; color: white; "
                "background-color: #a61b1b; padding: 10px;"
            )
            self.recording_ui.recording_state_label.setText("Recording in progress")
            self.recording_ui.recording_state_label.setStyleSheet(
                "font-size: 18px; font-weight: bold; color: #a61b1b;"
            )
            self.recording_ui.recording_primary_button.setEnabled(True)
        else:
            self.recording_ui.recording_primary_button.setText("Start recording")
            self.recording_ui.recording_primary_button.setStyleSheet(
                "font-size: 16px; font-weight: bold; color: white; "
                "background-color: #176b91; padding: 10px;"
            )
            self.recording_ui.recording_state_label.setText("Ready to record")
            self.recording_ui.recording_state_label.setStyleSheet(
                "font-size: 18px; font-weight: bold;"
            )
            self.recording_ui.recording_primary_button.setEnabled(
                self.workflow.can_start_recording()
            )

    def configure_sensor_mapping(self):
        if self.processes.backend_is_active():
            QMessageBox.warning(
                self,
                "Sensor system is running",
                "Stop the sensor system before changing sensor "
                "assignments. A changed mapping invalidates "
                "the current calibration.",
            )
            return

        try:
            mapping = load_sensor_mapping(SENSOR_MAPPING_FILE)
        except ValueError as error:
            QMessageBox.critical(
                self,
                "Cannot edit sensor mapping",
                str(error),
            )
            return

        dialog = SensorMappingDialog(
            mapping,
            SENSOR_MAPPING_FILE,
            self,
        )

        if dialog.exec() != QDialog.DialogCode.Accepted:
            return

        if not self.refresh_sensor_mapping(show_errors=True):
            return

        self.workflow.state.invalidate_calibration()

        self.update_session_summary()
        self.update_controls()

        self.status_ui.status_label.setText(
            "Sensor mapping saved — start the sensor system "
            "and perform a new static calibration"
        )

        self.status_ui.status_label.setStyleSheet(
            "padding: 8px;font-weight: bold;color: #8a6500;background-color: #fff4cc;"
        )

        QMessageBox.information(
            self,
            "Sensor mapping saved",
            "The new sensor assignments were saved.\n\n"
            "Start the sensor system and perform a new static "
            "calibration before recording.",
        )

    def update_sensor_connection_status(
        self,
        sensor_id,
        connected,
    ):
        segment = None

        for (
            mapped_segment,
            mapped_sensor_id,
        ) in self.workflow.state.sensor_mapping.items():
            if mapped_sensor_id.upper() == sensor_id.upper():
                segment = mapped_segment
                break

        if segment is None:
            return

        try:
            row = CANONICAL_SEGMENTS.index(segment)
        except ValueError:
            return

        status_item = QTableWidgetItem("Connected" if connected else "Disconnected")

        status_item.setTextAlignment(Qt.AlignmentFlag.AlignCenter)

        status_item.setForeground(QColor("#176b2c" if connected else "#8b1a1a"))

        self.patient_ui.sensor_mapping_table.setItem(
            row,
            2,
            status_item,
        )

    def refresh_sensor_mapping(
        self,
        show_errors=False,
    ):
        try:
            mapping = load_sensor_mapping(SENSOR_MAPPING_FILE)
            self.workflow.state.sensor_mapping = mapping

        except ValueError as error:
            self.patient_ui.sensor_mapping_status_label.setText(
                "Sensor mapping: invalid"
            )
            self.patient_ui.sensor_mapping_status_label.setStyleSheet(
                "color: #8b1a1a; font-weight: bold;"
            )

            self.patient_ui.sensor_mapping_table.clearContents()

            for row, segment in enumerate(CANONICAL_SEGMENTS):
                self.patient_ui.sensor_mapping_table.setItem(
                    row,
                    0,
                    QTableWidgetItem(segment),
                )

                self.patient_ui.sensor_mapping_table.setItem(
                    row,
                    1,
                    QTableWidgetItem("<unavailable>"),
                )
                self.patient_ui.sensor_mapping_table.setItem(
                    row,
                    2,
                    QTableWidgetItem("Unknown"),
                )

            if show_errors:
                QMessageBox.critical(
                    self,
                    "Invalid sensor mapping",
                    str(error),
                )
            self.workflow.state.sensor_mapping = {}

            return False

        for row, segment in enumerate(CANONICAL_SEGMENTS):
            segment_item = QTableWidgetItem(segment)

            sensor_item = QTableWidgetItem(mapping[segment].upper())

            is_connected = self.workflow.state.sensor_connection_status.get(
                segment,
                False,
            )

            status_item = QTableWidgetItem(
                "Connected" if is_connected else "Disconnected"
            )

            status_item.setTextAlignment(Qt.AlignmentFlag.AlignCenter)

            status_item.setForeground(QColor("#176b2c" if is_connected else "#8b1a1a"))

            self.patient_ui.sensor_mapping_table.setItem(
                row,
                0,
                segment_item,
            )
            self.patient_ui.sensor_mapping_table.setItem(
                row,
                1,
                sensor_item,
            )
            self.patient_ui.sensor_mapping_table.setItem(
                row,
                2,
                status_item,
            )

        self.patient_ui.sensor_mapping_status_label.setText(
            f"Sensor mapping: {EXPECTED_SENSOR_COUNT}/{EXPECTED_SENSOR_COUNT} assigned"
        )

        self.patient_ui.sensor_mapping_status_label.setToolTip(str(SENSOR_MAPPING_FILE))

        self.patient_ui.sensor_mapping_status_label.setStyleSheet(
            "color: #176b2c; font-weight: bold;"
        )

        return True

    def preflight_issues(self):
        return [str(issue) for issue in run_preflight_checks(APP_PATHS)]

    def run_system_check(self):
        issues = self.preflight_issues()

        if issues:
            message = "\n\n".join(issues)

            LOGGER.error(
                "Preflight failed:\n%s",
                message,
            )

            QMessageBox.critical(
                self,
                "System check failed",
                message,
            )

            self.status_ui.status_label.setText("System check failed")
            self.status_ui.status_label.setStyleSheet(
                "padding: 8px;"
                "font-weight: bold;"
                "color: #8b1a1a;"
                "background-color: #f8dddd;"
            )
            return False

        LOGGER.info("Preflight check passed.")

        QMessageBox.information(
            self,
            "System check passed",
            "All required executables, model files, "
            "sensor assignments and OpenSim paths "
            "were validated.",
        )

        self.status_ui.status_label.setText("System check passed")
        self.status_ui.status_label.setStyleSheet(
            "padding: 8px;font-weight: bold;color: #176b2c;background-color: #dff3e4;"
        )

        return True
