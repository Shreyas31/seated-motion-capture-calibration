"""Resolved frontend paths and process-wide runtime services."""

import os

from frontend.app_config import resolve_app_paths
from frontend.application_bootstrap import get_application_logger

APP_PATHS = resolve_app_paths()


def add_opensim_runtime_to_path():
    """Make the configured OpenSim DLL directory available to child processes."""
    if APP_PATHS.opensim_home is None:
        return

    opensim_bin_directory = APP_PATHS.opensim_home / "bin"
    if not opensim_bin_directory.is_dir():
        return

    current_path = os.environ.get("PATH", "")
    path_entries = [entry for entry in current_path.split(os.pathsep) if entry]
    normalized_entries = {
        os.path.normcase(os.path.abspath(entry)) for entry in path_entries
    }
    normalized_opensim_bin = os.path.normcase(
        os.path.abspath(str(opensim_bin_directory))
    )
    if normalized_opensim_bin not in normalized_entries:
        os.environ["PATH"] = str(opensim_bin_directory) + (
            os.pathsep + current_path if current_path else ""
        )


PROJECT_ROOT = APP_PATHS.project_root
SENSOR_MAPPING_FILE = APP_PATHS.sensor_mapping_file
DATA_DIRECTORY = APP_PATHS.data_directory
RAJAGOPAL_SOURCE_MODEL = APP_PATHS.rajagopal_source_model
BACKEND_EXE = APP_PATHS.backend_executable
VIEWER_EXE = APP_PATHS.viewer_executable
MODEL_GENERATOR_EXE = APP_PATHS.model_generator_executable
POSE_PRESETS_FILE = APP_PATHS.pose_presets_file
LOGGER = get_application_logger()
