"""Backend and viewer protocol handling for the main window."""

import html

from PySide6.QtCore import QTimer

from frontend.protocol import (
    Category,
    ErrorName,
    FrontendProtocolEvent,
    GuidanceName,
    ResultName,
    SensorStatus,
    ViewerEventName,
)
from frontend.runtime_context import LOGGER
from frontend.sensor_mapping import EXPECTED_SENSOR_COUNT


class EventsMixin:
    """Consumes child-process output and projects events into session state."""

    def read_backend_output(self, output):
        if output.strip():
            LOGGER.info(
                "[BACKEND] %s",
                output.rstrip(),
            )

        self.status_ui.backend_log.moveCursor(
            self.status_ui.backend_log.textCursor().MoveOperation.End
        )
        self.status_ui.backend_log.insertPlainText(output)
        self.status_ui.backend_log.ensureCursorVisible()

    def dispatch_backend_event(self, event):
        """Route one complete backend protocol event to workflow or guidance."""
        if event.category == Category.STATE:
            update = self.workflow.apply_backend_event(event)
            new_state = update.backend_state
            if new_state is None:
                return
            self.update_controls()
            self.guide_to_backend_state(new_state)
            if new_state == "sensors":
                self.continue_when_all_sensors_connected()
            if new_state == "joint":
                self.send_pending_functional_choice()
            if new_state == "menu":
                self.status_ui.guidance_label.setText(
                    "Guidance: Ready. Select a calibration or begin a recording trial."
                )
                if self.shutdown_requested:
                    QTimer.singleShot(0, self.continue_application_shutdown)
            elif new_state == "recording":
                self.status_ui.guidance_label.setText(
                    "Guidance: Recording is active. Ask the patient to perform "
                    "the required movement, then press Stop recording."
                )
        elif event.category in {Category.RESULT, Category.ERROR}:
            self.handle_backend_event(event.category, event.name, event.detail)
        elif event.category == Category.GUIDANCE:
            self.handle_backend_guidance(event.name, event.detail)

    def handle_backend_guidance(self, phase, detail):
        # Functional capture events use "movement name|seconds" so this GUI
        # reflects backend-configured timings instead of duplicating constants.
        functional_capture_name = detail
        functional_capture_seconds = "?"
        if phase in {
            GuidanceName.FUNCTIONAL_GRAVITY_CAPTURE,
            GuidanceName.FUNCTIONAL_MOVEMENT_CAPTURE,
            GuidanceName.FUNCTIONAL_RETURN_POSE_CAPTURE,
        }:
            try:
                functional_capture_name, functional_capture_seconds = detail.rsplit(
                    "|", 1
                )
            except ValueError:
                # Remain compatible with an older backend that sends only the
                # movement name.
                functional_capture_name = detail

        guidance_messages = {
            GuidanceName.STATIC_PREPARATION: (
                f"Place the patient in the {detail} pose. "
                "Use the chair, backrest or bed supports and "
                "ask the patient to remain still."
            ),
            GuidanceName.STATIC_CAPTURE: (
                f"Static calibration is recording for {detail} seconds. Do not move."
            ),
            GuidanceName.STATIC_PROCESSING: (
                "Static calibration captured. Checking all "
                f"{EXPECTED_SENSOR_COUNT} sensors..."
            ),
            GuidanceName.FUNCTIONAL_GRAVITY_PREPARATION: (
                f"Prepare {detail}. Return both connected "
                "segments to the static calibration pose "
                "and hold still."
            ),
            GuidanceName.FUNCTIONAL_GRAVITY_CAPTURE: (
                "Recording the stationary gravity phase for "
                f"{functional_capture_name} for {functional_capture_seconds} "
                "seconds. Do not move."
            ),
            GuidanceName.FUNCTIONAL_GRAVITY_PROCESSING: (
                f"Checking the stationary gravity data for {detail}."
            ),
            GuidanceName.FUNCTIONAL_MOVEMENT_PREPARATION: (
                (
                    f"Prepare {detail}. Keep the upper arm still, "
                    "the elbow near 90 degrees and the wrist neutral. "
                    "Begin with supination, then turn the palm up "
                    "and down smoothly."
                )
                if "pronation/supination" in detail.lower()
                else (
                    f"Prepare to move {detail} repeatedly through "
                    "comfortable flexion and extension. Avoid "
                    "movement outside the intended hinge plane."
                )
            ),
            GuidanceName.FUNCTIONAL_MOVEMENT_CAPTURE: (
                f"Recording {functional_capture_name} for "
                f"{functional_capture_seconds} seconds. Continue moving "
                "smoothly until instructed to stop."
            ),
            GuidanceName.FUNCTIONAL_RETURN_POSE_CAPTURE: (
                f"Recording the return pose for {functional_capture_name} "
                f"for {functional_capture_seconds} seconds. Do not move."
            ),
            GuidanceName.FUNCTIONAL_PROCESSING: (
                f"Functional capture complete for {detail}. "
                "Calculating the joint-axis refinement..."
            ),
        }

        if phase == GuidanceName.COUNTDOWN:
            try:
                countdown_phase, remaining = detail.rsplit(":", 1)
            except ValueError:
                countdown_phase = detail
                remaining = "?"

            phase_names = {
                "STATIC": "Static calibration",
                "FUNCTIONAL_GRAVITY": ("Stationary gravity capture"),
                "FUNCTIONAL_MOVEMENT": ("Functional movement capture"),
            }

            readable_phase = phase_names.get(
                countdown_phase,
                countdown_phase.replace("_", " ").title(),
            )

            message = f"{readable_phase} starts in {remaining}..."
        else:
            message = guidance_messages.get(
                phase,
                detail or phase.replace("_", " ").title(),
            )

        self.status_ui.guidance_label.setText(f"Guidance: {message}")

    def update_session_summary(self):
        expected_sensor_count = EXPECTED_SENSOR_COUNT

        mapped_connected_count = sum(
            1
            for connected in self.workflow.state.sensor_connection_status.values()
            if connected
        )

        if self.workflow.state.sensor_status_events_received:
            self.workflow.state.connected_sensor_count = mapped_connected_count
        else:
            mapped_connected_count = self.workflow.state.connected_sensor_count

        if mapped_connected_count == expected_sensor_count:
            self.status_ui.sensor_summary_label.setText(
                f"Sensors: {EXPECTED_SENSOR_COUNT}/{EXPECTED_SENSOR_COUNT} connected"
            )
            self.status_ui.sensor_summary_label.setStyleSheet(
                "color: #176b2c; font-weight: bold;"
            )
        elif mapped_connected_count > 0:
            self.status_ui.sensor_summary_label.setText(
                f"Sensors: {mapped_connected_count}/{expected_sensor_count} connected"
            )
            self.status_ui.sensor_summary_label.setStyleSheet(
                "color: #8b1a1a; font-weight: bold;"
            )
        else:
            self.status_ui.sensor_summary_label.setText("Sensors: not checked")
            self.status_ui.sensor_summary_label.setStyleSheet("")

        if self.workflow.state.static_calibrated:
            self.status_ui.static_summary_label.setText(
                f"Static calibration: completed ({self.workflow.state.static_pose})"
            )
            self.status_ui.static_summary_label.setStyleSheet(
                "color: #176b2c; font-weight: bold;"
            )
        else:
            self.status_ui.static_summary_label.setText(
                "Static calibration: not completed"
            )
            self.status_ui.static_summary_label.setStyleSheet("color: #8b1a1a;")

        functional_entries = []

        for joint_name in sorted(self.workflow.state.functional_calibrations):
            functional_entries.append(
                '<span style="color:#176b2c;'
                'font-weight:bold;">'
                f"{html.escape(joint_name)}: passed"
                "</span>"
            )

        for joint_name, reason in sorted(
            self.workflow.state.failed_functional_calibrations.items()
        ):
            functional_entries.append(
                '<span style="color:#8b1a1a;'
                'font-weight:bold;">'
                f"{html.escape(joint_name)}: failed"
                f" ({html.escape(reason)})"
                "</span>"
            )

        if functional_entries:
            self.status_ui.functional_summary_label.setText(
                "Functional calibrations: " + ", ".join(functional_entries)
            )
        else:
            self.status_ui.functional_summary_label.setText(
                "Functional calibrations: none"
            )

        self.status_ui.functional_summary_label.setStyleSheet("")

        if self.workflow.state.recording_active:
            self.status_ui.recording_summary_label.setText("Recording: active")
            self.status_ui.recording_summary_label.setStyleSheet(
                "color: #8b1a1a; font-weight: bold;"
            )
        else:
            self.status_ui.recording_summary_label.setText("Recording: stopped")
            self.status_ui.recording_summary_label.setStyleSheet("")

        self.update_sensor_overview()
        self.update_calibration_cards()

    def handle_backend_event(self, category, name, detail):
        LOGGER.info(
            "Backend event | %s | %s | %s",
            category,
            name,
            detail,
        )

        event = FrontendProtocolEvent(category, name, detail)
        update = self.workflow.apply_backend_event(event)
        readable_name = name.replace("_", " ").title()

        if category == Category.RESULT and name == ResultName.SENSOR_STATUS:
            try:
                sensor_id, sensor_state = detail.rsplit(":", 1)
            except ValueError:
                LOGGER.warning(
                    "Malformed sensor status event: %s",
                    detail,
                )
                return

            self.update_sensor_connection_status(
                sensor_id,
                sensor_state == SensorStatus.CONNECTED,
            )

            self.update_session_summary()
            self.update_controls()
            if update.request_sensor_continue:
                self.continue_when_all_sensors_connected()
            return

        if category == Category.RESULT and name == ResultName.UNMAPPED_SENSOR:
            unmapped_text = ", ".join(
                sorted(self.workflow.state.unmapped_connected_sensors)
            )

            self.patient_ui.sensor_mapping_status_label.setText(
                f"Sensor mapping: unassigned connected sensor(s): {unmapped_text}"
            )

            self.patient_ui.sensor_mapping_status_label.setStyleSheet(
                "color: #8a6500; font-weight: bold;"
            )

            self.update_sensor_overview()

            return

        if category == Category.RESULT:
            if name == ResultName.SESSION_CONFIGURED and not detail:
                self.workflow.state.confirmed_session_name = (
                    self.patient_ui.session_name_input.text().strip()
                )
            elif name == ResultName.RECORDING_STARTED:
                self.recording_elapsed_timer.restart()
                self.recording_ui.recording_elapsed_label.setText("00:00")
                self.recording_ui_timer.start()
                self.recording_ui.recording_output_label.setText(
                    "Recording is in progress."
                )

            elif name == ResultName.RECORDING_STOPPED:
                self.update_recording_elapsed()
                self.recording_ui_timer.stop()
                self.recording_ui.recording_output_label.setText(
                    "Recording completed and session outputs were saved."
                )

                if update.request_viewer_stop:
                    self.terminate_viewer_process()

                if self.shutdown_requested:
                    QTimer.singleShot(
                        0,
                        self.continue_application_shutdown,
                    )

        elif category == Category.ERROR and name == ErrorName.RECORDING:
            self.recording_ui_timer.stop()
            self.recording_ui.recording_output_label.setText(
                "Recording failed. Review the message above or open "
                "Diagnostics for technical details."
            )

            if self.processes.viewer_is_active():
                self.stop_viewer()

        self.update_session_summary()
        self.update_controls()

        if category == Category.RESULT:
            message = f"Success: {readable_name}"

            if detail:
                message += f" - {detail}"

            self.status_ui.status_label.setText(message)
            self.status_ui.status_label.setStyleSheet(
                "padding: 8px;"
                "font-weight: bold;"
                "color: #176b2c;"
                "background-color: #dff3e4;"
            )

        elif category == Category.ERROR:
            message = f"Error: {readable_name}"

            if detail:
                message += f" - {detail}"

            self.status_ui.status_label.setText(message)
            self.status_ui.status_label.setStyleSheet(
                "padding: 8px;"
                "font-weight: bold;"
                "color: #8b1a1a;"
                "background-color: #f8dddd;"
            )

    def read_viewer_output(self, output):
        if output.strip():
            LOGGER.info(
                "[OPENSIM] %s",
                output.rstrip(),
            )

        self.status_ui.viewer_log.moveCursor(
            self.status_ui.viewer_log.textCursor().MoveOperation.End
        )
        self.status_ui.viewer_log.insertPlainText(output)
        self.status_ui.viewer_log.ensureCursorVisible()

    def handle_viewer_event(self, event_name, detail):
        if event_name == ViewerEventName.READY:
            start_recording = self.workflow.mark_viewer_ready()
            self.update_controls()
            self.status_ui.status_label.setText(f"OpenSim viewer ready: {detail}")
            self.status_ui.status_label.setStyleSheet(
                "padding: 8px;font-weight: bold;color: #176b2c;"
                "background-color: #dff3e4;"
            )
            if start_recording:
                self.send_backend_value("4")
        elif event_name == ViewerEventName.STREAM_ENDED:
            self.workflow.mark_viewer_stream_ended()
            self.status_ui.status_label.setText("OpenSim stream finished")
            self.status_ui.status_label.setStyleSheet(
                "padding: 8px; font-weight: bold;"
            )
        elif event_name == ViewerEventName.CLOSED:
            self.workflow.mark_viewer_closed()
            self.status_ui.status_label.setText("OpenSim viewer window closed")
            self.status_ui.status_label.setStyleSheet(
                "padding: 8px; font-weight: bold;"
            )
            self.processes.terminate_viewer()
            self.update_controls()

    def viewer_finished(self, exit_code, exit_status):
        LOGGER.info(
            "OpenSim viewer finished with exit code %s",
            exit_code,
        )

        intentional_stop, was_waiting = self.workflow.mark_viewer_finished()
        self.update_controls()

        self.status_ui.viewer_log.appendPlainText(
            f"\nOpenSim viewer finished with exit code {exit_code}."
        )

        if intentional_stop:
            self.status_ui.status_label.setText("OpenSim viewer stopped")
            self.status_ui.status_label.setStyleSheet(
                "padding: 8px; font-weight: bold;"
            )

        if was_waiting:
            self.status_ui.status_label.setText(
                "OpenSim viewer exited before recording started"
            )
            self.status_ui.status_label.setStyleSheet(
                "padding: 8px;"
                "font-weight: bold;"
                "color: #8b1a1a;"
                "background-color: #f8dddd;"
            )

        if self.shutdown_requested:
            QTimer.singleShot(
                0,
                self.continue_application_shutdown,
            )

    def process_error(self, process_name, error):
        if self.shutdown_requested and error == "Crashed":
            LOGGER.info(
                "%s process ended during application shutdown.",
                process_name,
            )
            return

        if (
            process_name == "OpenSim viewer"
            and self.workflow.state.viewer_stop_requested
            and error == "Crashed"
        ):
            LOGGER.info("OpenSim viewer process terminated intentionally.")
            return

        message = f"{process_name} process error: {error}"

        LOGGER.error(message)

        self.status_ui.status_label.setText(message)
        self.status_ui.status_label.setStyleSheet(
            "padding: 8px;font-weight: bold;color: #8b1a1a;background-color: #f8dddd;"
        )

        if process_name in ("Backend", "Sensor system"):
            self.workflow.mark_backend_stopped()

        elif process_name == "OpenSim viewer":
            self.workflow.state.reset_viewer()

        elif process_name == "Model generator":
            self.workflow.state.reset_model()

        self.update_session_summary()
        self.update_controls()

    def backend_finished(self, exit_code, exit_status):
        LOGGER.info(
            "Backend finished with exit code %s",
            exit_code,
        )

        if self.processes.viewer_is_active():
            self.terminate_viewer_process()

        self.workflow.mark_backend_stopped()
        self.reset_session_controls()
        self.update_session_summary()
        self.status_ui.status_label.setText(
            f"Sensor system stopped with exit code {exit_code}"
        )
        self.status_ui.status_label.setStyleSheet("padding: 8px; font-weight: bold;")

        self.update_controls()

        self.status_ui.backend_log.appendPlainText(
            f"\nSensor system finished with exit code {exit_code}."
        )
        if self.shutdown_requested:
            QTimer.singleShot(
                0,
                self.continue_application_shutdown,
            )

    def reset_session_controls(self):
        """Restore clinician inputs and status widgets for a new session."""
        self.recording_ui_timer.stop()
        self.recording_ui.recording_elapsed_label.setText("00:00")
        self.recording_ui.recording_output_label.setText(
            "No recording has been made in this session."
        )
        self.recording_ui.recording_viewer_status_label.setText(
            "OpenSim viewer: stopped"
        )
        self.recording_ui.recording_viewer_status_label.setStyleSheet(
            "font-weight: bold; color: #6b7280;"
        )

        self.patient_ui.session_name_input.clear()
        self.patient_ui.pose_box.setCurrentIndex(0)
        self.patient_ui.height_input.setValue(1.70)
        self.patient_ui.foot_length_input.setValue(0.25)
        self.recording_ui.stationary_feet_checkbox.setChecked(False)

        self.patient_ui.model_summary_label.setText("Patient model: not generated")
        self.patient_ui.model_summary_label.setStyleSheet("color: #8b1a1a;")

        self.status_ui.guidance_label.setText(
            "Guidance: Start the sensor system, enter session details, "
            "and generate the patient model."
        )

    def update_controls(self):
        running = self.processes.backend_is_running()
        viewer_running = self.processes.viewer_is_active()
        model_generator_running = self.processes.model_is_running()
        self.workflow.update_process_states(
            backend=running,
            viewer=viewer_running,
            model=model_generator_running,
        )

        if self.shutdown_requested:
            for button in (
                self.patient_ui.start_backend_button,
                self.patient_ui.system_check_button,
                self.patient_ui.reload_sensor_mapping_button,
                self.patient_ui.configure_sensor_mapping_button,
                self.patient_ui.reset_headings_button,
                self.patient_ui.submit_session_button,
                self.patient_ui.generate_model_button,
                self.calibration_ui.static_calibration_button,
                self.calibration_ui.start_joint_button,
                self.recording_ui.start_viewer_button,
                self.recording_ui.stop_viewer_button,
                self.recording_ui.recording_primary_button,
            ):
                button.setEnabled(False)

            self.patient_ui.pose_box.setEnabled(False)
            self.calibration_ui.joint_box.setEnabled(False)
            self.patient_ui.session_name_input.setEnabled(False)
            self.patient_ui.height_input.setEnabled(False)
            self.patient_ui.foot_length_input.setEnabled(False)
            self.recording_ui.stationary_feet_checkbox.setEnabled(False)
            return

        patient_inputs_enabled = (
            not model_generator_running
            and not viewer_running
            and not self.workflow.state.recording_active
        )

        self.patient_ui.start_backend_button.setEnabled(not running)

        self.patient_ui.reload_sensor_mapping_button.setEnabled(not running)

        self.patient_ui.configure_sensor_mapping_button.setEnabled(not running)

        heading_enabled = running and self.workflow.state.backend_state == "heading"
        self.patient_ui.reset_headings_button.setEnabled(heading_enabled)

        session_name_editable = (
            patient_inputs_enabled
            and self.workflow.state.confirmed_session_name is None
        )
        self.patient_ui.session_name_input.setEnabled(session_name_editable)
        self.patient_ui.session_name_input.setToolTip(
            ""
            if session_name_editable
            else "The session name is locked after confirmation. "
            "Start a new sensor-system session to use a different name."
        )

        confirm_session_enabled = self.workflow.can_confirm_session() and bool(
            self.patient_ui.session_name_input.text().strip()
        )

        self.patient_ui.submit_session_button.setEnabled(confirm_session_enabled)

        self.patient_ui.height_input.setEnabled(patient_inputs_enabled)

        self.patient_ui.foot_length_input.setEnabled(patient_inputs_enabled)

        menu_enabled = self.workflow.can_start_static_calibration()

        self.calibration_ui.static_calibration_button.setEnabled(menu_enabled)

        functional_enabled = self.workflow.can_start_functional_calibration()

        self.calibration_ui.joint_box.setEnabled(functional_enabled)
        self.calibration_ui.start_joint_button.setEnabled(functional_enabled)

        self.recording_ui.exit_backend_button.setEnabled(menu_enabled)

        self.patient_ui.pose_box.setEnabled(patient_inputs_enabled)

        self.recording_ui.stationary_feet_checkbox.setEnabled(
            not viewer_running and not self.workflow.state.recording_active
        )

        self.patient_ui.generate_model_button.setEnabled(
            not model_generator_running
            and not viewer_running
            and not self.workflow.state.recording_active
            and bool(self.patient_ui.session_name_input.text().strip())
        )

        self.recording_ui.start_viewer_button.setEnabled(
            self.workflow.state.model_prepared
            and not model_generator_running
            and not viewer_running
            and not self.workflow.state.recording_active
        )

        self.recording_ui.stop_viewer_button.setEnabled(viewer_running)

        self.update_workflow_header()
        self.update_compact_session_summary()
        self.update_workflow_navigation()
        self.update_recording_page()

    def process_states_changed(self):
        """Refresh workflow gates after any child-process state transition."""
        self.update_controls()
