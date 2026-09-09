"""Ordered shutdown behavior for frontend-managed child processes."""

from PySide6.QtWidgets import QMessageBox

from frontend.runtime_context import LOGGER


class ShutdownControllerMixin:
    """Coordinates safe recording, viewer, model, and backend shutdown."""

    def exit_backend_safely(self):
        response = QMessageBox.question(
            self,
            "Exit sensor system?",
            "Exit the acquisition session? The current calibration will be cleared.",
            (QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No),
            QMessageBox.StandardButton.No,
        )

        if response == QMessageBox.StandardButton.Yes:
            self.send_backend_value("5")

    def managed_processes_are_active(self):
        """Return whether any frontend-managed process is active."""
        return self.processes.any_process_is_active()

    def application_exit_warning(self):
        """Describe work that will be interrupted by application exit."""
        consequences = []

        if self.workflow.state.recording_active:
            consequences.append(
                "The active recording will be stopped and its "
                "CSV files will be finalized."
            )
        elif (
            self.processes.backend_is_running()
            and self.workflow.state.backend_state not in {"menu", "stopped"}
        ):
            consequences.append("The current sensor-system activity will be cancelled.")

        if self.processes.viewer_is_active():
            consequences.append("The OpenSim viewer will be closed.")

        if self.processes.model_is_active():
            consequences.append("Patient-model generation will be stopped.")

        if self.processes.backend_is_active():
            consequences.append(
                "The sensor-system session and current calibration will be cleared."
            )

        if not consequences:
            return "Exit the application?"

        return "\n\n".join(consequences) + "\n\nExit the application?"

    def request_application_exit(self):
        """Confirm and begin an orderly application shutdown."""
        if self.shutdown_requested:
            return

        response = QMessageBox.question(
            self,
            "Exit IRP Seated Motion Capture?",
            self.application_exit_warning(),
            (QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No),
            QMessageBox.StandardButton.No,
        )

        if response != QMessageBox.StandardButton.Yes:
            return

        self.shutdown_requested = True
        self.backend_exit_sent = False
        self.recording_stop_sent_for_shutdown = False

        self.update_controls()

        self.status_ui.status_label.setText("Closing safely…")
        self.status_ui.status_label.setStyleSheet("padding: 8px; font-weight: bold;")

        self.shutdown_timer.start()
        self.continue_application_shutdown()

    def continue_application_shutdown(self):
        """Advance shutdown according to the current process states."""
        if not self.shutdown_requested:
            return

        if self.shutdown_complete:
            return

        backend_active = self.processes.backend_is_active()

        # Recording must finish first so the backend can flush and close
        # all active CSV files before any process is terminated.
        if backend_active and self.workflow.state.recording_active:
            if not self.recording_stop_sent_for_shutdown:
                self.recording_stop_sent_for_shutdown = True
                self.status_ui.status_label.setText(
                    "Stopping recording and saving output files…"
                )
                self.send_backend_value("0")

            return

        # Recording has ended, so the viewer can now close.
        if self.processes.viewer_is_active():
            self.status_ui.status_label.setText("Closing OpenSim viewer…")
            self.terminate_viewer_process()

        # Model generation cannot be safely completed after the UI exits.
        if self.processes.model_is_active():
            self.status_ui.status_label.setText("Stopping patient-model generation…")
            self.processes.terminate_model()

        backend_active = self.processes.backend_is_active()

        if backend_active:
            if self.backend_exit_sent:
                # The backend has already received option 6. Wait for its
                # finished signal rather than sending another command.
                return

            if self.workflow.state.backend_state == "menu":
                self.status_ui.status_label.setText("Closing sensor-system session…")
                self.backend_exit_sent = True
                self.send_backend_value("5")
                return

            # The backend has no global cancellation command for sensor
            # connection, heading, session, static capture or functional
            # capture prompts. The clinician has explicitly confirmed that
            # this unfinished activity may be discarded.
            self.status_ui.status_label.setText(
                "Cancelling the current sensor-system activity…"
            )
            self.processes.terminate_backend()
            return

        self.finish_application_shutdown_if_ready()

    def finish_application_shutdown_if_ready(self):
        """Close the window once every managed process has stopped."""
        if not self.shutdown_requested:
            return

        if self.managed_processes_are_active():
            return

        self.shutdown_timer.stop()
        self.shutdown_complete = True

        LOGGER.info("Frontend and managed processes closed normally.")

        self.close()

    def force_application_shutdown(self):
        """Kill child processes that did not stop within the timeout."""
        if not self.shutdown_requested:
            return

        LOGGER.warning("Safe shutdown timed out; terminating remaining processes.")

        self.processes.kill_all()

        self.workflow.state.recording_active = False
        self.recording_ui_timer.stop()

        self.shutdown_complete = True
        self.close()

    def closeEvent(self, event):
        """Route title-bar closure through the managed shutdown workflow."""
        if self.shutdown_complete:
            event.accept()
            return

        if not self.managed_processes_are_active():
            LOGGER.info("Frontend closed normally.")
            event.accept()
            return

        event.ignore()
        self.request_application_exit()
