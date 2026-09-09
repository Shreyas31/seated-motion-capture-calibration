from pathlib import Path

from frontend.process_supervisor import (
    ProcessSupervisor,
    build_model_generator_command,
    build_sensor_system_command,
    build_viewer_command,
)
from frontend.protocol import FrontendProtocolEvent
from frontend.sensor_mapping import EXPECTED_SENSOR_COUNT


def test_backend_protocol_is_parsed_after_complete_line(qtbot):
    supervisor = ProcessSupervisor()
    events = []
    supervisor.backend_event.connect(events.append)

    remaining = supervisor._consume_lines(
        "FRONTEND|RESULT|SENSORS_",
        f"CONNECTED|{EXPECTED_SENSOR_COUNT}\nordinary output",
        supervisor._handle_backend_line,
    )

    assert events == [
        FrontendProtocolEvent("RESULT", "SENSORS_CONNECTED", str(EXPECTED_SENSOR_COUNT))
    ]
    assert remaining == "ordinary output"


def test_viewer_ready_has_specific_and_generic_signals(qtbot):
    supervisor = ProcessSupervisor()
    events = []
    ready = []
    supervisor.viewer_event.connect(lambda name, detail: events.append((name, detail)))
    supervisor.viewer_ready.connect(lambda: ready.append(True))

    supervisor._handle_viewer_line("FRONTEND|VIEWER|READY|chair_palms_together")

    assert events == [("READY", "chair_palms_together")]
    assert ready == [True]


def test_process_state_helpers_start_inactive(qtbot):
    supervisor = ProcessSupervisor()

    assert not supervisor.backend_is_active()
    assert not supervisor.backend_is_running()
    assert not supervisor.viewer_is_active()
    assert not supervisor.model_is_active()
    assert not supervisor.model_is_running()
    assert not supervisor.any_process_is_active()


def test_sensor_system_command_contains_mapping_and_output_directory():
    command = build_sensor_system_command(
        executable=Path("bin/backend.exe"),
        project_root=Path("project"),
        sensor_mapping_file=Path("config/sensors.json"),
        data_directory=Path("measurements"),
    )

    assert command.arguments == ("--sensor-map", "config\\sensors.json")
    assert command.environment_overrides == (
        ("SEATED_MOCAP_OUTPUT_DIR", "measurements"),
    )
    assert command.working_directory == Path("project")


def test_model_generator_command_preserves_required_argument_order():
    command = build_model_generator_command(
        executable=Path("generator.exe"),
        project_root=Path("project"),
        source_model=Path("source.osim"),
        output_model=Path("patient.osim"),
        source_geometry=Path("Geometry"),
        anthropometry_file=Path("patient.json"),
    )

    assert command.arguments == (
        "source.osim",
        "patient.osim",
        "Geometry",
        "patient.json",
    )


def test_viewer_preview_uses_free_root_without_output_file():
    command = build_viewer_command(
        executable=Path("viewer.exe"),
        project_root=Path("project"),
        model_file=Path("patient.osim"),
        pose_presets_file=Path("poses.json"),
        pose_name="chair_palms_together",
        geometry_directory=Path("Geometry"),
        stationary_feet=False,
    )

    assert command.arguments[-2:] == ("3600", "free-root")
    assert len(command.arguments) == 6


def test_viewer_recording_appends_output_and_stationary_feet_mode():
    command = build_viewer_command(
        executable=Path("viewer.exe"),
        project_root=Path("project"),
        model_file=Path("patient.osim"),
        pose_presets_file=Path("poses.json"),
        pose_name="bed_palms_together",
        geometry_directory=Path("Geometry"),
        stationary_feet=True,
        joint_angle_output=Path("angles.csv"),
    )

    assert command.arguments[-2:] == ("stationary-feet", "angles.csv")
    assert len(command.arguments) == 7
