"""Native backend, model-generator, and viewer process commands."""

from PySide6.QtCore import QTimer
from PySide6.QtWidgets import QMessageBox

from frontend.process_supervisor import (
    build_model_generator_command,
    build_sensor_system_command,
    build_viewer_command,
)
from frontend.runtime_context import (
    APP_PATHS,
    BACKEND_EXE,
    DATA_DIRECTORY,
    LOGGER,
    MODEL_GENERATOR_EXE,
    POSE_PRESETS_FILE,
    PROJECT_ROOT,
    RAJAGOPAL_SOURCE_MODEL,
    SENSOR_MAPPING_FILE,
    VIEWER_EXE,
)
from frontend.sensor_mapping import EXPECTED_SENSOR_COUNT
from frontend.session_files import (
    PatientAnthropometry,
    next_joint_angle_csv_path,
    patient_model_files,
    write_patient_anthropometry,
)


class ExecutionMixin:
    """Starts child processes and sends validated workflow commands."""

    def start_backend(self):
        if self.processes.backend_is_active():
            QMessageBox.information(
                self,
                "Sensor system running",
                "The sensor system is already running.",
            )
            return

        if not self.refresh_sensor_mapping(show_errors=True):
            return

        if not BACKEND_EXE.is_file():
            QMessageBox.critical(
                self,
                "Missing sensor system",
                f"Sensor-system executable not found:\n{BACKEND_EXE}",
            )
            return

        self.status_ui.backend_log.clear()
        self.status_ui.backend_log.appendPlainText("Starting sensor system...\n")

        self.workflow.reset_for_backend_start()

        self.refresh_sensor_mapping(show_errors=False)

        self.patient_ui.model_summary_label.setText("Patient model: not generated")
        self.patient_ui.model_summary_label.setStyleSheet("color: #8b1a1a;")
        self.update_session_summary()
        self.update_controls()

        command = build_sensor_system_command(
            executable=BACKEND_EXE,
            project_root=PROJECT_ROOT,
            sensor_mapping_file=SENSOR_MAPPING_FILE,
            data_directory=DATA_DIRECTORY,
        )
        self.processes.start_backend(command)

    def patient_input_signature(self):
        subject_id = self.patient_ui.session_name_input.text().strip()

        height = round(
            self.patient_ui.height_input.value(),
            3,
        )

        foot_length = round(
            self.patient_ui.foot_length_input.value(),
            3,
        )

        return (
            subject_id,
            height,
            foot_length,
        )

    def patient_inputs_changed(self, *args):
        current_inputs = self.patient_input_signature()

        if (
            self.workflow.state.model_prepared
            and self.workflow.state.prepared_model_inputs != current_inputs
        ):
            self.workflow.state.invalidate_model()

            self.patient_ui.model_summary_label.setText(
                "Patient model: measurements changed - regeneration required"
            )
            self.patient_ui.model_summary_label.setStyleSheet(
                "color: #8b1a1a; font-weight: bold;"
            )

            self.status_ui.status_label.setText(
                "Patient measurements changed; regenerate the OpenSim model"
            )
            self.status_ui.status_label.setStyleSheet(
                "padding: 8px;"
                "font-weight: bold;"
                "color: #8b1a1a;"
                "background-color: #f8dddd;"
            )

        self.update_controls()

    def generate_patient_model(self):
        if self.processes.model_is_active():
            QMessageBox.information(
                self,
                "Model generation running",
                "Patient model generation is already running.",
            )
            return

        subject_id = self.patient_ui.session_name_input.text().strip()

        if not subject_id:
            QMessageBox.warning(
                self,
                "Missing subject ID",
                "Enter the session or subject ID first.",
            )
            return

        if not MODEL_GENERATOR_EXE.is_file():
            QMessageBox.critical(
                self,
                "Missing model generator",
                f"Executable not found:\n{MODEL_GENERATOR_EXE}",
            )
            return

        if not RAJAGOPAL_SOURCE_MODEL.is_file():
            QMessageBox.critical(
                self,
                "Missing Rajagopal model",
                f"Rajagopal_2015.osim was not found:\n{RAJAGOPAL_SOURCE_MODEL}",
            )
            return

        opensim_home = APP_PATHS.opensim_home

        if not opensim_home:
            QMessageBox.critical(
                self,
                "OPENSIM_HOME missing",
                "OPENSIM_HOME is not defined.",
            )
            return

        source_geometry = opensim_home / "Geometry"

        if not source_geometry.is_dir():
            QMessageBox.critical(
                self,
                "Missing geometry",
                f"Geometry directory not found:\n{source_geometry}",
            )
            return

        try:
            model_files = patient_model_files(
                DATA_DIRECTORY,
                subject_id,
            )
        except ValueError as error:
            QMessageBox.warning(
                self,
                "Invalid subject ID",
                str(error),
            )
            return

        foot_length = self.patient_ui.foot_length_input.value()

        anthropometry = PatientAnthropometry(
            subject_id=subject_id,
            patient_height=self.patient_ui.height_input.value(),
            foot_length=foot_length,
        )

        try:
            write_patient_anthropometry(
                model_files.anthropometry_file,
                anthropometry,
            )
        except OSError as error:
            QMessageBox.critical(
                self,
                "Could not write measurements",
                str(error),
            )
            return

        command = build_model_generator_command(
            executable=MODEL_GENERATOR_EXE,
            project_root=PROJECT_ROOT,
            source_model=RAJAGOPAL_SOURCE_MODEL,
            output_model=model_files.model_file,
            source_geometry=source_geometry,
            anthropometry_file=model_files.anthropometry_file,
        )

        self.workflow.state.pending_model_inputs = self.patient_input_signature()

        self.workflow.state.model_prepared = False
        self.workflow.state.pending_model_output = model_files.model_file

        self.patient_ui.model_summary_label.setText("Patient model: generating...")
        self.patient_ui.model_summary_label.setStyleSheet(
            "color: #8a6500; font-weight: bold;"
        )

        self.status_ui.status_label.setText(
            "Generating patient-specific OpenSim model..."
        )
        self.status_ui.status_label.setStyleSheet("padding: 8px; font-weight: bold;")

        self.status_ui.viewer_log.appendPlainText("\nGenerating patient model...")

        self.update_controls()

        self.processes.start_model(command)

    def read_model_output(self, output):
        if output.strip():
            LOGGER.info(
                "[MODEL] %s",
                output.rstrip(),
            )

        self.status_ui.viewer_log.moveCursor(
            self.status_ui.viewer_log.textCursor().MoveOperation.End
        )
        self.status_ui.viewer_log.insertPlainText("[MODEL] " + output)
        self.status_ui.viewer_log.ensureCursorVisible()

    def model_generation_finished(
        self,
        exit_code,
        exit_status,
    ):
        LOGGER.info(
            "Model generator finished with exit code %s",
            exit_code,
        )

        output_model = self.workflow.state.pending_model_output

        inputs_still_match = (
            self.workflow.state.pending_model_inputs == self.patient_input_signature()
        )

        generation_succeeded = (
            exit_code == 0
            and output_model is not None
            and output_model.is_file()
            and inputs_still_match
        )

        if generation_succeeded:
            self.workflow.state.current_model_file = output_model
            self.workflow.state.current_geometry_directory = (
                output_model.parent / "Geometry"
            )
            self.workflow.state.model_prepared = True
            self.workflow.state.prepared_model_inputs = (
                self.workflow.state.pending_model_inputs
            )

            subject_id, height, foot_length = self.workflow.state.prepared_model_inputs

            foot_text = f"{foot_length:.3f} m"

            self.patient_ui.model_summary_label.setText(
                "Patient model: ready\n"
                f"Subject: {subject_id} | "
                f"Height: {height:.3f} m | "
                f"Foot: {foot_text}"
            )

            self.patient_ui.model_summary_label.setToolTip(str(output_model))

            self.patient_ui.model_summary_label.setStyleSheet(
                "color: #176b2c; font-weight: bold;"
            )

            self.status_ui.status_label.setText(
                "Patient-specific OpenSim model generated"
            )
            self.status_ui.status_label.setStyleSheet(
                "padding: 8px;"
                "font-weight: bold;"
                "color: #176b2c;"
                "background-color: #dff3e4;"
            )
        else:
            self.workflow.state.invalidate_model()

            self.patient_ui.model_summary_label.setText(
                f"Patient model: generation failed (exit code {exit_code})"
            )
            self.patient_ui.model_summary_label.setStyleSheet(
                "color: #8b1a1a; font-weight: bold;"
            )

            self.status_ui.status_label.setText(
                "Patient OpenSim model generation failed"
            )
            self.status_ui.status_label.setStyleSheet(
                "padding: 8px;"
                "font-weight: bold;"
                "color: #8b1a1a;"
                "background-color: #f8dddd;"
            )

        self.workflow.state.clear_pending_model_generation()
        if self.shutdown_requested:
            QTimer.singleShot(
                0,
                self.continue_application_shutdown,
            )
        self.update_controls()

    def start_viewer(self, start_recording_when_ready=False):
        if self.processes.viewer_is_active():
            if not self.workflow.state.viewer_ready:
                # The visualizer was closed but its host process has not yet
                # completed shutdown.
                self.terminate_viewer_process()

            elif start_recording_when_ready:
                self.send_backend_value("4")
                return

            else:
                QMessageBox.information(
                    self,
                    "Viewer running",
                    "The OpenSim viewer is already running.",
                )
                return

        if (
            not self.workflow.state.model_prepared
            or self.workflow.state.current_model_file is None
            or self.workflow.state.current_geometry_directory is None
        ):
            QMessageBox.warning(
                self,
                "Patient model not prepared",
                "Generate the patient OpenSim model first.",
            )
            return

        model_file = self.workflow.state.current_model_file
        geometry_directory = self.workflow.state.current_geometry_directory

        required_paths = [
            VIEWER_EXE,
            model_file,
            POSE_PRESETS_FILE,
            geometry_directory,
        ]

        missing = [path for path in required_paths if not path.exists()]
        if missing:
            QMessageBox.critical(
                self,
                "Missing OpenSim file",
                "The following path does not exist:\n"
                + "\n".join(str(path) for path in missing),
            )
            return

        if (
            self.workflow.state.static_calibrated
            and self.workflow.state.static_pose_preset is not None
        ):
            pose_name = self.workflow.state.static_pose_preset
        else:
            pose = self.patient_ui.pose_box.currentData()
            pose_name = f"{pose}_palms_together"

        stationary_feet = self.recording_ui.stationary_feet_checkbox.isChecked()

        joint_angle_output = None

        if start_recording_when_ready:
            joint_angle_output = next_joint_angle_csv_path(
                DATA_DIRECTORY,
                self.workflow.state.confirmed_session_name
                or self.patient_ui.session_name_input.text().strip(),
            )

        command = build_viewer_command(
            executable=VIEWER_EXE,
            project_root=PROJECT_ROOT,
            model_file=model_file,
            pose_presets_file=POSE_PRESETS_FILE,
            pose_name=pose_name,
            geometry_directory=geometry_directory,
            stationary_feet=stationary_feet,
            joint_angle_output=joint_angle_output,
        )

        self.workflow.state.viewer_joint_angle_output = joint_angle_output

        self.status_ui.viewer_log.clear()
        self.status_ui.viewer_log.appendPlainText(
            f"Starting OpenSim viewer for {pose_name} pose.\n"
            f"Stationary-feet translation: "
            f"{'enabled' if stationary_feet else 'disabled'}\n"
            + (
                f"Joint-angle output: {joint_angle_output}\n"
                if joint_angle_output is not None
                else "Joint-angle recording: disabled for preview\n"
            )
        )

        self.workflow.state.viewer_ready = False
        self.workflow.state.pending_recording_start = start_recording_when_ready

        self.status_ui.status_label.setText("Starting OpenSim viewer...")
        self.status_ui.status_label.setStyleSheet("padding: 8px; font-weight: bold;")

        self.workflow.state.viewer_stop_requested = False
        self.processes.start_viewer(command)
        self.update_controls()

    def backend_is_running(self):
        return self.processes.backend_is_running()

    def send_backend_value(self, value):
        if not self.processes.backend_is_running():
            QMessageBox.warning(
                self,
                "Sensor system not running",
                "Start the sensor system first.",
            )
            return

        self.workflow.state.backend_state = "busy"
        self.update_controls()

        self.processes.write_backend(str(value))

    def warn_missing_prerequisite(self, title, message):
        """Explain why an action cannot run without changing backend state."""
        QMessageBox.warning(self, title, message)
        self.status_ui.status_label.setText(message)
        self.status_ui.status_label.setStyleSheet(
            "padding: 8px;font-weight: bold;color: #8b1a1a;background-color: #f8dddd;"
        )
        return False

    def require_running_backend_state(self, expected_state, action_name):
        """Prevent a command from being written to the wrong backend prompt."""
        if not self.processes.backend_is_running():
            return self.warn_missing_prerequisite(
                "Start the sensor system",
                f"Start the sensor system on the Setup page before you {action_name}.",
            )

        if self.workflow.state.backend_state != expected_state:
            return self.warn_missing_prerequisite(
                "Complete the current step first",
                f"You cannot {action_name} yet. Complete the guided sensor "
                "system prompts shown under Current step, then try again.",
            )

        return True

    def continue_when_all_sensors_connected(self):
        """Advance once when every mapped sensor is connected."""
        if not self.backend_is_running():
            return

        if self.workflow.state.sensor_continue_sent:
            return

        if self.workflow.state.connected_sensor_count != EXPECTED_SENSOR_COUNT:
            return

        if self.workflow.state.backend_state != "sensors":
            return

        self.workflow.state.sensor_continue_sent = True

        self.status_ui.status_label.setText(
            f"All {EXPECTED_SENSOR_COUNT} mapped sensors are connected. "
            "Continuing automatically."
        )

        self.send_backend_value("")

    def resolve_heading_offsets(self, response):
        if not self.require_running_backend_state(
            "heading", "choose a heading-offset option"
        ):
            return

        self.send_backend_value(response)

    def submit_session_name(self):
        session_name = self.patient_ui.session_name_input.text().strip()

        if not session_name:
            QMessageBox.warning(
                self,
                "Missing session name",
                "Enter a session name before continuing.",
            )
            return

        if not self.workflow.state.model_prepared:
            self.warn_missing_prerequisite(
                "Patient model not prepared",
                "Generate the patient OpenSim model on the Setup page "
                "before confirming the session.",
            )
            return

        if self.workflow.state.connected_sensor_count != EXPECTED_SENSOR_COUNT:
            self.warn_missing_prerequisite(
                "Sensors not ready",
                f"Connect all {EXPECTED_SENSOR_COUNT} mapped sensors before "
                "confirming the session.",
            )
            return

        if not self.require_running_backend_state("session", "confirm the session"):
            return

        self.send_backend_value(session_name)

    def require_calibration_prerequisites(self, action_name):
        if not self.workflow.state.model_prepared:
            return self.warn_missing_prerequisite(
                "Patient model not prepared",
                f"Generate the patient OpenSim model before you {action_name}.",
            )

        if self.workflow.state.connected_sensor_count != EXPECTED_SENSOR_COUNT:
            return self.warn_missing_prerequisite(
                "Sensors not ready",
                f"Connect all {EXPECTED_SENSOR_COUNT} mapped sensors before "
                f"you {action_name}.",
            )

        return self.require_running_backend_state("menu", action_name)

    def start_static_calibration(self):
        if not self.require_calibration_prerequisites("perform static calibration"):
            return

        pose = self.patient_ui.pose_box.currentData()

        readable_pose = {
            "chair": "chair-seated",
            "bed": "bed-seated",
        }[pose]

        confirmation_text = (
            f"Confirm that the patient is ready for the "
            f"{readable_pose} static calibration with their "
            "palms facing each other."
        )

        response = QMessageBox.question(
            self,
            "Confirm static calibration",
            confirmation_text,
            (QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No),
            QMessageBox.StandardButton.No,
        )

        if response != QMessageBox.StandardButton.Yes:
            return

        backend_choice = {
            "chair": "1",
            "bed": "2",
        }[pose]

        self.workflow.state.pending_static_pose_preset = f"{pose}_palms_together"

        self.send_backend_value(backend_choice)

    def start_functional_joint(self):
        """Begin the selected functional calibration from the backend menu."""
        if not self.workflow.state.static_calibrated:
            self.warn_missing_prerequisite(
                "Static calibration required",
                "Complete a successful static calibration before "
                "starting a functional calibration.",
            )
            return

        if self.workflow.state.pending_functional_choice is not None:
            self.warn_missing_prerequisite(
                "Functional calibration starting",
                "The selected functional calibration is already being "
                "sent to the sensor system.",
            )
            return

        if not self.require_calibration_prerequisites(
            "start the selected functional calibration"
        ):
            return

        joint_name = self.calibration_ui.joint_box.currentText()
        joint_choice = self.calibration_ui.joint_box.currentData()

        if joint_choice is None:
            self.warn_missing_prerequisite(
                "No movement selected",
                "Select a functional movement before starting.",
            )
            return

        response = QMessageBox.question(
            self,
            "Start functional calibration?",
            f"Begin paired functional calibration for {joint_name}?",
            (QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No),
            QMessageBox.StandardButton.No,
        )

        if response != QMessageBox.StandardButton.Yes:
            return

        # The backend uses a two-stage terminal protocol:
        # menu option 4 enters functional calibration, after which
        # STATE|JOINT requests the movement number.
        self.workflow.state.pending_functional_choice = int(joint_choice)

        self.status_ui.status_label.setText(
            f"Starting functional calibration: {joint_name}"
        )

        self.send_backend_value("3")

    def send_pending_functional_choice(self):
        """Send the queued movement when the backend requests a joint."""
        if self.workflow.state.backend_state != "joint":
            return

        joint_choice = self.workflow.state.pending_functional_choice

        if joint_choice is None:
            return

        # Clear before sending so a repeated STATE|JOINT event cannot
        # submit the same movement twice.
        self.workflow.state.pending_functional_choice = None

        self.send_backend_value(str(joint_choice))

    def start_recording(self):
        if not self.workflow.state.model_prepared:
            self.warn_missing_prerequisite(
                "Patient model not prepared",
                "Generate the patient OpenSim model before recording.",
            )
            return

        if self.workflow.state.connected_sensor_count != EXPECTED_SENSOR_COUNT:
            self.warn_missing_prerequisite(
                "Sensors not ready",
                f"Connect all {EXPECTED_SENSOR_COUNT} mapped sensors before recording.",
            )
            return

        if not self.workflow.state.static_calibrated:
            self.warn_missing_prerequisite(
                "Static calibration required",
                "Complete a successful static calibration before recording.",
            )
            return

        if not self.require_running_backend_state("menu", "start recording"):
            return

        if self.processes.viewer_is_active() and self.workflow.state.viewer_ready:
            if self.workflow.state.viewer_joint_angle_output is not None:
                self.send_backend_value("4")
                return

            # The current viewer is preview-only. Restart it with a joint-angle
            # output path before beginning the recording.
            self.terminate_viewer_process()

        self.workflow.state.pending_recording_start = True

        self.status_ui.status_label.setText(
            "Waiting for OpenSim viewer to become ready..."
        )
        self.status_ui.status_label.setStyleSheet("padding: 8px; font-weight: bold;")

        self.start_viewer(start_recording_when_ready=True)

    def stop_recording(self):
        if not self.require_running_backend_state("recording", "stop recording"):
            return

        self.send_backend_value("0")

    def terminate_viewer_process(self):
        if not self.processes.viewer_is_active():
            return

        self.workflow.state.viewer_ready = False
        self.workflow.state.pending_recording_start = False
        self.workflow.state.viewer_stop_requested = True
        self.processes.terminate_viewer()

        self.update_controls()

    def stop_viewer(self):
        if not self.processes.viewer_is_active():
            return

        if self.workflow.state.recording_active:
            response = QMessageBox.question(
                self,
                "Stop recording and viewer?",
                "A recording is active. Stop the recording "
                "and close the OpenSim viewer?",
                (QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No),
                QMessageBox.StandardButton.No,
            )

            if response != QMessageBox.StandardButton.Yes:
                return

            # Wait until the backend has closed its CSV and UDP
            # stream before terminating the viewer.
            self.workflow.state.pending_viewer_stop = True
            self.send_backend_value("0")
            return

        self.workflow.state.pending_viewer_stop = False
        self.workflow.state.viewer_stop_requested = False
        self.terminate_viewer_process()
