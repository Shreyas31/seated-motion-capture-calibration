"""Lifecycle management for frontend child processes."""

from __future__ import annotations

from collections.abc import Callable
from dataclasses import dataclass
from pathlib import Path

from PySide6.QtCore import QObject, QProcess, QProcessEnvironment, QTimer, Signal

from frontend.protocol import (
    PROTOCOL_PREFIX,
    FrontendProtocolEvent,
    ViewerEventName,
    parse_frontend_protocol_line,
)


@dataclass(frozen=True, slots=True)
class ProcessCommand:
    executable: Path
    arguments: tuple[str, ...]
    working_directory: Path
    environment_overrides: tuple[tuple[str, str], ...] = ()


def build_sensor_system_command(
    *,
    executable: Path,
    project_root: Path,
    sensor_mapping_file: Path,
    data_directory: Path,
) -> ProcessCommand:
    return ProcessCommand(
        executable=executable,
        arguments=("--sensor-map", str(sensor_mapping_file)),
        working_directory=project_root,
        environment_overrides=(("SEATED_MOCAP_OUTPUT_DIR", str(data_directory)),),
    )


def build_model_generator_command(
    *,
    executable: Path,
    project_root: Path,
    source_model: Path,
    output_model: Path,
    source_geometry: Path,
    anthropometry_file: Path,
) -> ProcessCommand:
    return ProcessCommand(
        executable=executable,
        arguments=(
            str(source_model),
            str(output_model),
            str(source_geometry),
            str(anthropometry_file),
        ),
        working_directory=project_root,
    )


def build_viewer_command(
    *,
    executable: Path,
    project_root: Path,
    model_file: Path,
    pose_presets_file: Path,
    pose_name: str,
    geometry_directory: Path,
    stationary_feet: bool,
    joint_angle_output: Path | None = None,
    timeout_seconds: int = 3600,
) -> ProcessCommand:
    arguments = (
        str(model_file),
        str(pose_presets_file),
        pose_name,
        str(geometry_directory),
        str(timeout_seconds),
        "stationary-feet" if stationary_feet else "free-root",
    )

    if joint_angle_output is not None:
        arguments += (str(joint_angle_output),)

    return ProcessCommand(
        executable=executable,
        arguments=arguments,
        working_directory=project_root,
    )


class ProcessSupervisor(QObject):
    """Own and supervise the sensor system, viewer, and model generator."""

    backend_output = Signal(str)
    backend_event = Signal(FrontendProtocolEvent)
    backend_finished = Signal(int)
    viewer_output = Signal(str)
    viewer_event = Signal(str, str)
    viewer_ready = Signal()
    viewer_finished = Signal(int)
    model_output = Signal(str)
    model_finished = Signal(int)
    process_error = Signal(str, str)
    state_changed = Signal()

    def __init__(self, parent: QObject | None = None) -> None:
        super().__init__(parent)
        self.backend = self._create_process("Sensor system")
        self.viewer = self._create_process("OpenSim viewer")
        self.model = self._create_process("Model generator")
        self._backend_buffer = ""
        self._viewer_buffer = ""

        self.backend.readyReadStandardOutput.connect(self._read_backend_output)
        self.viewer.readyReadStandardOutput.connect(self._read_viewer_output)
        self.model.readyReadStandardOutput.connect(self._read_model_output)
        self.backend.finished.connect(
            lambda exit_code, _status: self.backend_finished.emit(exit_code)
        )
        self.viewer.finished.connect(
            lambda exit_code, _status: self.viewer_finished.emit(exit_code)
        )
        self.model.finished.connect(
            lambda exit_code, _status: self.model_finished.emit(exit_code)
        )

    def _create_process(self, display_name: str) -> QProcess:
        process = QProcess(self)
        process.setProcessChannelMode(QProcess.ProcessChannelMode.MergedChannels)
        process.stateChanged.connect(lambda _state: self.state_changed.emit())
        process.errorOccurred.connect(
            lambda error, name=display_name: self.process_error.emit(
                name, getattr(error, "name", str(error))
            )
        )
        return process

    @staticmethod
    def _start(process: QProcess, command: ProcessCommand) -> None:
        environment = QProcessEnvironment.systemEnvironment()
        for name, value in command.environment_overrides:
            environment.insert(name, value)
        process.setProcessEnvironment(environment)
        process.setWorkingDirectory(str(command.working_directory))
        process.start(str(command.executable), list(command.arguments))

    def start_backend(self, command: ProcessCommand) -> None:
        self._backend_buffer = ""
        self._start(self.backend, command)

    def start_viewer(self, command: ProcessCommand) -> None:
        self._viewer_buffer = ""
        self._start(self.viewer, command)

    def start_model(self, command: ProcessCommand) -> None:
        self._start(self.model, command)

    def write_backend(self, value: str) -> None:
        self.backend.write((value + "\n").encode("utf-8"))

    def backend_is_running(self) -> bool:
        return self.backend.state() == QProcess.ProcessState.Running

    def backend_is_active(self) -> bool:
        return self.backend.state() != QProcess.ProcessState.NotRunning

    def viewer_is_active(self) -> bool:
        return self.viewer.state() != QProcess.ProcessState.NotRunning

    def model_is_running(self) -> bool:
        return self.model.state() == QProcess.ProcessState.Running

    def model_is_active(self) -> bool:
        return self.model.state() != QProcess.ProcessState.NotRunning

    def any_process_is_active(self) -> bool:
        return any(
            process.state() != QProcess.ProcessState.NotRunning
            for process in (self.backend, self.viewer, self.model)
        )

    def terminate_viewer(self, *, kill_after_ms: int = 1500) -> None:
        self._terminate_with_fallback(self.viewer, kill_after_ms)

    def terminate_model(self, *, kill_after_ms: int = 1500) -> None:
        self._terminate_with_fallback(self.model, kill_after_ms)

    def terminate_backend(self, *, kill_after_ms: int = 1500) -> None:
        self._terminate_with_fallback(self.backend, kill_after_ms)

    def kill_all(self, wait_ms: int = 1000) -> None:
        for process in (self.viewer, self.model, self.backend):
            if process.state() != QProcess.ProcessState.NotRunning:
                process.kill()
                process.waitForFinished(wait_ms)

    @staticmethod
    def _terminate_with_fallback(process: QProcess, kill_after_ms: int) -> None:
        if process.state() == QProcess.ProcessState.NotRunning:
            return
        process.terminate()

        def kill_if_running() -> None:
            if process.state() != QProcess.ProcessState.NotRunning:
                process.kill()

        QTimer.singleShot(kill_after_ms, kill_if_running)

    @staticmethod
    def _read_text(process: QProcess) -> str:
        return bytes(process.readAllStandardOutput()).decode("utf-8", errors="replace")

    @staticmethod
    def _consume_lines(
        buffer: str,
        output: str,
        handle_line: Callable[[str], None],
    ) -> str:
        buffer += output
        while "\n" in buffer:
            line, buffer = buffer.split("\n", 1)
            handle_line(line.rstrip("\r"))
        return buffer

    def _read_backend_output(self) -> None:
        output = self._read_text(self.backend)
        self.backend_output.emit(output)
        self._backend_buffer = self._consume_lines(
            self._backend_buffer, output, self._handle_backend_line
        )

    def _handle_backend_line(self, line: str) -> None:
        event = parse_frontend_protocol_line(line)
        if event is not None:
            self.backend_event.emit(event)

    def _read_viewer_output(self) -> None:
        output = self._read_text(self.viewer)
        self.viewer_output.emit(output)
        self._viewer_buffer = self._consume_lines(
            self._viewer_buffer, output, self._handle_viewer_line
        )

    def _handle_viewer_line(self, line: str) -> None:
        if not line.startswith(f"{PROTOCOL_PREFIX}|VIEWER|"):
            return
        parts = line.split("|", 3)
        if len(parts) < 3:
            return
        event_name = parts[2]
        detail = parts[3] if len(parts) == 4 else ""
        self.viewer_event.emit(event_name, detail)
        if event_name == ViewerEventName.READY:
            self.viewer_ready.emit()

    def _read_model_output(self) -> None:
        self.model_output.emit(self._read_text(self.model))
