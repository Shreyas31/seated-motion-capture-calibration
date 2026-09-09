import os
from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import Path

from frontend.app_config import AppPaths
from frontend.sensor_mapping import load_sensor_mapping


@dataclass(frozen=True, slots=True)
class DiagnosticIssue:
    summary: str
    detail: str | None = None

    def __str__(self) -> str:
        if self.detail is None:
            return self.summary

        return f"{self.summary}:\n{self.detail}"


def run_preflight_checks(
    paths: AppPaths,
    *,
    environment: Mapping[str, str] | None = None,
) -> tuple[DiagnosticIssue, ...]:
    environment_values = os.environ if environment is None else environment
    issues: list[DiagnosticIssue] = []

    required_files = (
        ("Sensor-system executable not found", paths.backend_executable),
        ("Sensor mapping not found", paths.sensor_mapping_file),
        ("OpenSim viewer executable not found", paths.viewer_executable),
        (
            "Model-generator executable not found",
            paths.model_generator_executable,
        ),
        ("Pose presets not found", paths.pose_presets_file),
        ("Rajagopal source model not found", paths.rajagopal_source_model),
    )

    for summary, required_path in required_files:
        if not required_path.is_file():
            issues.append(DiagnosticIssue(summary, str(required_path)))

    if paths.sensor_mapping_file.is_file():
        try:
            load_sensor_mapping(paths.sensor_mapping_file)
        except ValueError as error:
            issues.append(DiagnosticIssue(str(error)))

    if paths.opensim_home is None:
        issues.append(
            DiagnosticIssue(
                "OpenSim runtime was not found",
                "Install a packaged runtime or define OPENSIM_HOME.",
            )
        )
    else:
        geometry_directory = paths.opensim_home / "Geometry"

        if not geometry_directory.is_dir():
            issues.append(
                DiagnosticIssue(
                    "OpenSim Geometry directory not found",
                    str(geometry_directory),
                )
            )

        opensim_bin = paths.opensim_home / "bin"
        path_entries = {
            Path(entry)
            for entry in environment_values.get("PATH", "").split(os.pathsep)
            if entry
        }

        if opensim_bin not in path_entries:
            issues.append(
                DiagnosticIssue(
                    "OpenSim bin is not present in PATH",
                    str(opensim_bin),
                )
            )

    try:
        paths.generated_directory.mkdir(parents=True, exist_ok=True)
    except OSError as error:
        issues.append(
            DiagnosticIssue(
                "Generated-data directory is not writable",
                str(error),
            )
        )

    return tuple(issues)
