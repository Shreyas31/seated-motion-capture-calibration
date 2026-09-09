import os
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Mapping


@dataclass(frozen=True, slots=True)
class AppPaths:
    """Filesystem locations required by the clinician frontend."""

    project_root: Path
    sensor_mapping_file: Path
    data_directory: Path
    rajagopal_source_model: Path
    backend_executable: Path
    viewer_executable: Path
    model_generator_executable: Path
    pose_presets_file: Path
    opensim_home: Path | None
    installed_layout: bool

    @property
    def generated_directory(self) -> Path:
        # Generated patient models are user data. Keeping them outside the
        # installation directory also works for non-administrator installs.
        return self.data_directory / "models"

    @property
    def log_directory(self) -> Path:
        return self.data_directory / "logs"


def resolve_app_paths(
    *,
    project_root: Path | None = None,
    environ: Mapping[str, str] | None = None,
    home_directory: Path | None = None,
    executable_path: Path | None = None,
    frozen: bool | None = None,
) -> AppPaths:
    """Resolve deployment or development paths without changing the filesystem."""

    environment = os.environ if environ is None else environ
    is_frozen = bool(getattr(sys, "frozen", False)) if frozen is None else frozen
    if project_root is not None:
        root = Path(project_root)
    elif is_frozen:
        # PyInstaller places the executable beside bin/, resources/ and
        # runtime/. sys.executable is stable even when its Python modules are
        # unpacked into PyInstaller's private _internal directory.
        root = Path(executable_path or sys.executable).resolve().parent
    else:
        root = Path(__file__).resolve().parents[1]
    home = Path.home() if home_directory is None else Path(home_directory)

    sensor_mapping_file = Path(
        environment.get(
            "SEATED_MOCAP_SENSOR_MAP",
            str(root / "config" / "sensor_mapping.json"),
        )
    )
    data_directory = Path(
        environment.get(
            "SEATED_MOCAP_OUTPUT_DIR",
            str(home / "Documents" / "IRP_SeatedMoCap_Data"),
        )
    )
    bundled_source_model = root / "resources" / "Rajagopal_2015.osim"
    legacy_source_model = (
        home
        / "Documents"
        / "OpenSim"
        / "4.5"
        / "Code"
        / "Python"
        / "OpenSenseExample"
        / "Rajagopal_2015.osim"
    )
    default_source_model = (
        bundled_source_model if bundled_source_model.is_file() else legacy_source_model
    )
    rajagopal_source_model = Path(
        environment.get("RAJAGOPAL_SOURCE_MODEL", str(default_source_model))
    )

    install_bin_directory = root / "bin"
    installed_layout = (install_bin_directory / "backend.exe").is_file()

    if installed_layout:
        backend_executable = install_bin_directory / "backend.exe"
        viewer_executable = install_bin_directory / "opensim_rt_viewer.exe"
        model_generator_executable = (
            install_bin_directory / "generate_seated_mocap_model.exe"
        )
        pose_presets_file = root / "resources" / "pose_presets.json"
    else:
        development_build_directory = (
            root / "out" / "build" / "x64-opensim-relwithdebinfo"
        )

        backend_executable = (
            development_build_directory / "backend" / "RelWithDebInfo" / "backend.exe"
        )
        viewer_executable = (
            development_build_directory
            / "opensim"
            / "RelWithDebInfo"
            / "opensim_rt_viewer.exe"
        )
        model_generator_executable = (
            development_build_directory
            / "opensim"
            / "RelWithDebInfo"
            / "generate_seated_mocap_model.exe"
        )
        pose_presets_file = root / "generated" / "pose_presets.json"

    opensim_home_value = environment.get("OPENSIM_HOME")
    bundled_opensim_home = root / "runtime" / "OpenSim"
    opensim_home = (
        Path(opensim_home_value)
        if opensim_home_value
        else bundled_opensim_home
        if bundled_opensim_home.is_dir()
        else None
    )

    return AppPaths(
        project_root=root,
        sensor_mapping_file=sensor_mapping_file,
        data_directory=data_directory,
        rajagopal_source_model=rajagopal_source_model,
        backend_executable=backend_executable,
        viewer_executable=viewer_executable,
        model_generator_executable=model_generator_executable,
        pose_presets_file=pose_presets_file,
        opensim_home=opensim_home,
        installed_layout=installed_layout,
    )
