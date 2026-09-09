"""Frontend composition root and Qt application entry point."""

import sys

from PySide6.QtCore import QElapsedTimer, QTimer
from PySide6.QtWidgets import QApplication, QMainWindow

from frontend.application_bootstrap import (
    configure_application_logging,
    load_application_stylesheet,
)
from frontend.calibration_page import CalibrationPageHandlers, build_calibration_page
from frontend.events_controller import EventsMixin
from frontend.execution_controller import ExecutionMixin
from frontend.presentation_controller import PresentationMixin
from frontend.process_supervisor import ProcessSupervisor
from frontend.recording_page import RecordingPageHandlers, build_recording_page
from frontend.runtime_context import DATA_DIRECTORY, LOGGER, add_opensim_runtime_to_path
from frontend.sensor_mapping import CANONICAL_SEGMENTS
from frontend.setup_page import SetupPageHandlers, build_setup_page
from frontend.shutdown_controller import ShutdownControllerMixin
from frontend.workflow_controller import WorkflowController
from frontend.workflow_ui import build_diagnostics_page


class MainWindow(
    PresentationMixin,
    ExecutionMixin,
    EventsMixin,
    ShutdownControllerMixin,
    QMainWindow,
):
    """Composes the clinician UI with workflow and process controllers."""

    def __init__(self):
        super().__init__()

        self.setWindowTitle("IRP Seated Motion Capture")
        self.resize(1000, 700)

        self.processes = ProcessSupervisor(self)
        self.workflow = WorkflowController()
        self.shutdown_requested = False
        self.shutdown_complete = False
        self.backend_exit_sent = False
        self.recording_stop_sent_for_shutdown = False

        self.shutdown_timer = QTimer(self)
        self.shutdown_timer.setSingleShot(True)
        self.shutdown_timer.setInterval(10000)
        self.shutdown_timer.timeout.connect(self.force_application_shutdown)
        self.recording_elapsed_timer = QElapsedTimer()
        self.recording_ui_timer = QTimer(self)
        self.recording_ui_timer.setInterval(1000)
        self.recording_ui_timer.timeout.connect(self.update_recording_elapsed)

        self.processes.backend_output.connect(self.read_backend_output)
        self.processes.backend_event.connect(self.dispatch_backend_event)
        self.processes.backend_finished.connect(
            lambda exit_code: self.backend_finished(exit_code, None)
        )
        self.processes.viewer_output.connect(self.read_viewer_output)
        self.processes.viewer_event.connect(self.handle_viewer_event)
        self.processes.viewer_finished.connect(
            lambda exit_code: self.viewer_finished(exit_code, None)
        )
        self.processes.model_output.connect(self.read_model_output)
        self.processes.model_finished.connect(
            lambda exit_code: self.model_generation_finished(exit_code, None)
        )
        self.processes.process_error.connect(self.process_error)
        self.processes.state_changed.connect(self.process_states_changed)

        self.patient_ui = build_setup_page(
            SetupPageHandlers(
                update_controls=self.update_controls,
                start_sensor_system=self.start_backend,
                run_system_check=self.run_system_check,
                reload_sensor_mapping=lambda: self.refresh_sensor_mapping(
                    show_errors=True
                ),
                configure_sensor_mapping=self.configure_sensor_mapping,
                toggle_sensor_table=self.toggle_sensor_table,
                reset_heading_offsets=lambda: self.resolve_heading_offsets("y"),
                submit_session=self.submit_session_name,
                generate_model=self.generate_patient_model,
                patient_inputs_changed=self.patient_inputs_changed,
            ),
            CANONICAL_SEGMENTS,
        )
        self.calibration_ui = build_calibration_page(
            CalibrationPageHandlers(
                start_static_calibration=self.start_static_calibration,
                start_functional_calibration=self.start_functional_joint,
            )
        )
        self.recording_ui = build_recording_page(
            RecordingPageHandlers(
                start_viewer=self.start_viewer,
                stop_viewer=self.stop_viewer,
                toggle_recording=self.toggle_recording_from_primary,
                exit_sensor_system=self.exit_backend_safely,
            )
        )
        self.status_ui = build_diagnostics_page()

        self.update_session_summary()

        self.build_main_layout()

        self.refresh_sensor_mapping(show_errors=False)

        self.update_controls()


def main() -> int:
    """Start the clinician desktop application."""
    add_opensim_runtime_to_path()
    configure_application_logging(DATA_DIRECTORY)
    application = QApplication(sys.argv)
    load_application_stylesheet(
        application,
        logger=LOGGER,
    )
    window = MainWindow()
    window.show()
    return application.exec()


if __name__ == "__main__":
    sys.exit(main())
