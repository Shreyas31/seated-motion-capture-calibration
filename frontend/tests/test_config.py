from pathlib import Path

from frontend.app_config import resolve_app_paths


def test_resolves_development_defaults(tmp_path):
    project_root = tmp_path / "project"
    home = tmp_path / "home"

    paths = resolve_app_paths(
        project_root=project_root,
        environ={},
        home_directory=home,
    )

    build = project_root / "out" / "build" / "x64-opensim-relwithdebinfo"
    assert not paths.installed_layout
    assert paths.sensor_mapping_file == (
        project_root / "config" / "sensor_mapping.json"
    )
    assert paths.data_directory == (home / "Documents" / "IRP_SeatedMoCap_Data")
    assert paths.backend_executable == (
        build / "backend" / "RelWithDebInfo" / "backend.exe"
    )
    assert paths.viewer_executable == (
        build / "opensim" / "RelWithDebInfo" / "opensim_rt_viewer.exe"
    )
    assert paths.pose_presets_file == (project_root / "generated" / "pose_presets.json")
    assert paths.opensim_home is None


def test_resolves_installed_layout(tmp_path):
    project_root = tmp_path / "installed"
    bin_directory = project_root / "bin"
    bin_directory.mkdir(parents=True)
    (bin_directory / "backend.exe").touch()

    paths = resolve_app_paths(
        project_root=project_root,
        environ={},
        home_directory=tmp_path / "home",
    )

    assert paths.installed_layout
    assert paths.backend_executable == bin_directory / "backend.exe"
    assert paths.viewer_executable == bin_directory / "opensim_rt_viewer.exe"
    assert paths.model_generator_executable == (
        bin_directory / "generate_seated_mocap_model.exe"
    )
    assert paths.pose_presets_file == (project_root / "resources" / "pose_presets.json")
    assert paths.generated_directory == paths.data_directory / "models"


def test_frozen_executable_defines_application_root(tmp_path):
    install_root = tmp_path / "installed"
    (install_root / "bin").mkdir(parents=True)
    (install_root / "bin" / "backend.exe").touch()

    paths = resolve_app_paths(
        environ={},
        home_directory=tmp_path / "home",
        executable_path=install_root / "IRP_SeatedMoCap.exe",
        frozen=True,
    )

    assert paths.project_root == install_root
    assert paths.installed_layout


def test_installed_layout_prefers_bundled_runtime_and_model(tmp_path):
    install_root = tmp_path / "installed"
    (install_root / "bin").mkdir(parents=True)
    (install_root / "bin" / "backend.exe").touch()
    bundled_opensim = install_root / "runtime" / "OpenSim"
    bundled_opensim.mkdir(parents=True)
    bundled_model = install_root / "resources" / "Rajagopal_2015.osim"
    bundled_model.parent.mkdir(parents=True)
    bundled_model.touch()

    paths = resolve_app_paths(
        project_root=install_root,
        environ={},
        home_directory=tmp_path / "home",
    )

    assert paths.opensim_home == bundled_opensim
    assert paths.rajagopal_source_model == bundled_model


def test_environment_overrides_are_used(tmp_path):
    project_root = tmp_path / "project"
    environment = {
        "SEATED_MOCAP_SENSOR_MAP": str(tmp_path / "mapping.json"),
        "SEATED_MOCAP_OUTPUT_DIR": str(tmp_path / "measurements"),
        "RAJAGOPAL_SOURCE_MODEL": str(tmp_path / "source.osim"),
        "OPENSIM_HOME": str(tmp_path / "OpenSim"),
    }

    paths = resolve_app_paths(
        project_root=project_root,
        environ=environment,
        home_directory=tmp_path / "home",
    )

    assert paths.sensor_mapping_file == Path(environment["SEATED_MOCAP_SENSOR_MAP"])
    assert paths.data_directory == Path(environment["SEATED_MOCAP_OUTPUT_DIR"])
    assert paths.rajagopal_source_model == Path(environment["RAJAGOPAL_SOURCE_MODEL"])
    assert paths.opensim_home == Path(environment["OPENSIM_HOME"])


def test_resolution_has_no_directory_creation_side_effect(tmp_path):
    output_directory = tmp_path / "not-created"

    paths = resolve_app_paths(
        project_root=tmp_path / "project",
        environ={"SEATED_MOCAP_OUTPUT_DIR": str(output_directory)},
        home_directory=tmp_path / "home",
    )

    assert paths.data_directory == output_directory
    assert not output_directory.exists()
