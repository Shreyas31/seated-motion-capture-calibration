import os
from pathlib import Path

from frontend.app_config import AppPaths
from frontend.sensor_mapping import CANONICAL_SEGMENTS, save_sensor_mapping
from frontend.system_check import DiagnosticIssue, run_preflight_checks


def make_paths(tmp_path, *, opensim_home=True):
    project_root = tmp_path / "project"
    bin_directory = project_root / "bin"
    resources_directory = project_root / "resources"
    config_directory = project_root / "config"
    source_directory = tmp_path / "source"

    paths = AppPaths(
        project_root=project_root,
        sensor_mapping_file=config_directory / "sensor_mapping.json",
        data_directory=tmp_path / "measurements",
        rajagopal_source_model=source_directory / "Rajagopal_2015.osim",
        backend_executable=bin_directory / "backend.exe",
        viewer_executable=bin_directory / "opensim_rt_viewer.exe",
        model_generator_executable=(bin_directory / "generate_seated_mocap_model.exe"),
        pose_presets_file=resources_directory / "pose_presets.json",
        opensim_home=(tmp_path / "OpenSim" if opensim_home else None),
        installed_layout=True,
    )

    for required_file in (
        paths.backend_executable,
        paths.viewer_executable,
        paths.model_generator_executable,
        paths.pose_presets_file,
        paths.rajagopal_source_model,
    ):
        required_file.parent.mkdir(parents=True, exist_ok=True)
        required_file.touch()

    save_sensor_mapping(
        paths.sensor_mapping_file,
        {
            segment: f"sensor-{index:02d}"
            for index, segment in enumerate(CANONICAL_SEGMENTS, start=1)
        },
    )

    if paths.opensim_home is not None:
        (paths.opensim_home / "Geometry").mkdir(parents=True)
        (paths.opensim_home / "bin").mkdir()

    return paths


def test_diagnostic_issue_formats_optional_detail():
    assert str(DiagnosticIssue("Missing", "file.txt")) == "Missing:\nfile.txt"
    assert str(DiagnosticIssue("Unavailable")) == "Unavailable"


def test_complete_system_has_no_issues(tmp_path):
    paths = make_paths(tmp_path)
    environment = {"PATH": str(paths.opensim_home / "bin")}

    assert run_preflight_checks(paths, environment=environment) == ()
    assert paths.generated_directory.is_dir()


def test_missing_required_file_is_reported(tmp_path):
    paths = make_paths(tmp_path)
    paths.viewer_executable.unlink()
    environment = {"PATH": str(paths.opensim_home / "bin")}

    issues = run_preflight_checks(paths, environment=environment)

    assert any("OpenSim viewer executable" in str(issue) for issue in issues)


def test_missing_opensim_configuration_is_reported(tmp_path):
    paths = make_paths(tmp_path, opensim_home=False)

    issues = run_preflight_checks(paths, environment={"PATH": ""})

    assert (
        DiagnosticIssue(
            "OpenSim runtime was not found",
            "Install a packaged runtime or define OPENSIM_HOME.",
        )
        in issues
    )


def test_missing_opensim_bin_path_is_reported(tmp_path):
    paths = make_paths(tmp_path)
    unrelated_path = str(Path(tmp_path) / "unrelated")

    issues = run_preflight_checks(
        paths,
        environment={"PATH": unrelated_path + os.pathsep},
    )

    assert any("not present in PATH" in str(issue) for issue in issues)
