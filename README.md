# Seated Motion Capture

IRP Seated Motion Capture is a Windows application for collecting seated
motion-capture data from a 17-sensor Xsens Awinda system. It guides a clinician
through sensor checks, patient-specific OpenSim model generation, static and
optional functional calibration, measurement recording, and real-time OpenSim
visualisation.

The repository contains three cooperating programs:

| Component | Technology | Responsibility |
| --- | --- | --- |
| Clinician frontend | Python 3 and PySide6 | Guides the workflow and manages the native processes. |
| Sensor backend | C++20 and Xsens Device API | Connects the Awinda system, calibrates IMUs, writes measurements, and streams orientations. |
| OpenSim tools | C++20 and OpenSim | Generate patient models and run real-time inverse kinematics and visualisation. |

The normal runtime data flow is:

```text
PySide6 frontend
    |-- stdin/stdout ------------> Xsens backend
    |                                  |-- measurement/relative/calibration CSVs
    |                                  `-- UDP orientation frames (127.0.0.1:9001)
    |                                                           |
    |-- starts patient-model generator                           v
    `-- starts OpenSim viewer --------------------------> real-time IK and joint angles
```

See [Architecture](docs/architecture.md) for the complete workflow and the
recommended code-reading order.

## Repository layout

| Path | Contents |
| --- | --- |
| `frontend/` | PySide6 application, process orchestration, UI builders, and Python tests. |
| `backend/` | Xsens acquisition, calibration, recording workflows, and C++ tests. |
| `opensim/` | Model-generation utilities, pose presets, real-time viewer, and OpenSim tests. |
| `dependencies/` | Vendored Eigen, nlohmann/json, and Xsens SDK files used by the native programs. |
| `shared/include/` | Binary UDP and CSV contracts shared by the native programs. |
| `config/` | Sensor-to-segment assignments used by the frontend and backend. |
| `docs/` | Architecture, data contracts, testing, and validation guidance. |
| `generated/pose_presets.json` | Pose definitions required by the development viewer and release package. |
| `deployment/` | Release assembly, PyInstaller, and Inno Setup files. |
| `out/`, `.venv/`, `.vs/`, `release/` | Local build/runtime state; these are not authored source. |

Patient recordings, generated patient models, and logs are runtime data. Keep
them out of this application source repository. A separately governed pilot
dataset is described under [Evaluation material](#evaluation-material).

## Prerequisites

Development is currently validated on x64 Windows. Install or provide:

- Python 3.11 or newer.
- CMake 3.16 or newer.
- A Visual Studio/MSVC C++ environment compatible with the generators in
  `CMakePresets.json`. The OpenSim preset currently uses Visual Studio 18 2026
  with the v143 toolset.
- Ninja for the backend-only presets.
- OpenSim 4.5 with its C++ SDK.
- The Rajagopal 2015 source model used by the patient-model generator.
- The Xsens Awinda Windows driver for hardware use. The required Xsens SDK
  headers, libraries, runtime DLLs, and notices are under
  `dependencies/xsens`.

Set the OpenSim installation for development builds:

```powershell
$env:OPENSIM_HOME = "C:\OpenSim 4.5"
```

If the Rajagopal model is not at the default OpenSense example location, set:

```powershell
$env:RAJAGOPAL_SOURCE_MODEL = "C:\path\to\Rajagopal_2015.osim"
```

## Set up the Python frontend

From the repository root:

```powershell
py -3.11 -m venv .venv
.\.venv\Scripts\python.exe -m pip install --upgrade pip
.\.venv\Scripts\python.exe -m pip install -r frontend\requirements-dev.txt
```

`requirements-dev.txt` includes the runtime dependency, pytest, pytest-qt, and
Ruff. Use `frontend/requirements.txt` instead when only runtime dependencies are
needed.

## Build the native programs

### Full application build

The frontend's development paths expect the full OpenSim build layout. Run from
a Visual Studio developer PowerShell:

```powershell
$env:OPENSIM_HOME = "C:\OpenSim 4.5"
cmake --preset x64-opensim-relwithdebinfo
cmake --build --preset x64-opensim-relwithdebinfo
```

This builds the backend, patient-model tools, viewer, and native tests.

### Backend-only build

Use this when changing backend logic that does not require OpenSim:

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug
```

The `x64-debug` preset uses Ninja and requires `cl.exe` to be available in the
current shell.

## Run the application

After the Python environment and full native build exist:

```powershell
.\launch_frontend.cmd
```

Development mode resolves native executables beneath
`out/build/x64-opensim-relwithdebinfo`, reads
`generated/pose_presets.json`, and uses `config/sensor_mapping.json`.

The application writes runtime data beneath:

```text
%USERPROFILE%\Documents\IRP_SeatedMoCap_Data
```

The location can be overridden with `SEATED_MOCAP_OUTPUT_DIR`. Other supported
path overrides are documented in [Architecture](docs/architecture.md#runtime-paths).

## Run automated checks

Python tests and lint checks do not require Xsens hardware:

```powershell
.\.venv\Scripts\python.exe -m ruff check frontend
.\.venv\Scripts\python.exe -m pytest
```

Run native tests from the configured OpenSim build:

```powershell
ctest --preset x64-opensim-relwithdebinfo
```

The repository currently collects 132 Python tests and registers 24 C++ tests.
See [Testing](docs/testing.md) for test scopes, individual commands, and
hardware/release validation.

To run Python lint/tests and a complete configure/build/test cycle with one
command:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\check.ps1 `
    -Preset x64-debug
```

Use `-Preset x64-opensim-relwithdebinfo` for the full native build. The script
accepts `-SkipPython` when validating native code before a Python environment
has been created and `-SkipFormat` for a build-only check when the Visual Studio
C++ Clang tools are unavailable.

## Development guidance

- Treat `frontend/protocol.py`, `backend/frontend_protocol`,
  `shared/include/realtime_orientation_packet.h`, and
  `shared/include/csv_schemas.h` as compatibility boundaries.
- Do not change sensor ordering, protocol tokens, CSV headers, coordinate-frame
  conventions, or filenames without updating both producers and consumers and
  adding contract tests.
- Keep participant data outside source control.
- Preserve Doxygen descriptions for public header classes and functions,
  including `@param` tags and `@return` tags for non-void functions.
- Run automated tests after every change to calibration or workflow state.
- Complete the manual hardware checklist before distributing a release.

Protocol and data-format details are in
[Protocols and data](docs/protocols-and-data.md). Installer and recipient
instructions remain in [deployment/README.md](deployment/README.md).

## Source-control policy

Commit the authored source, tests, configuration, documentation, build scripts,
`generated/pose_presets.json`, and the vendored Eigen, nlohmann/json, and Xsens
dependencies with their notices. Do not commit participant data to this source
repository. Also exclude logs, virtual environments, compiler or CMake output,
generated patient models or geometry,
release packages, or IDE state. The root `.gitignore` enforces these exclusions
for normal Git workflows.

## Suggested reading order

1. `frontend/app_config.py`, `frontend/process_supervisor.py`, and
   `frontend/protocol.py` for runtime and process contracts.
2. `frontend/session_state.py` and `frontend/workflow_controller.py` for
   non-visual workflow state and allowed transitions.
3. `frontend/setup_page.py`, `frontend/calibration_page.py`,
   `frontend/recording_page.py`, and `frontend/workflow_ui.py` for the
   page-oriented UI structure.
4. `frontend/app.py`, followed by `presentation_controller.py`,
   `execution_controller.py`, `events_controller.py`, and
   `shutdown_controller.py` for UI composition and user actions.
5. `backend/src/main.cpp`, `application.cpp`, `application_bootstrap.cpp`, and
   `acquisition_workflow.cpp`.
6. The shared protocol and CSV headers.
7. `static_calibration_workflow`, then the functional-calibration files in this
   order: `functional_calibration_types`, `functional_calibration_capture`,
   `functional_calibration_solver`, `functional_return_pose`, and
   `functional_calibration_workflow`; finish with
   `measurement_recording_workflow`.
8. `opensim_rt_viewer.cpp`, `realtime_viewer_runner.cpp`, and
   `realtime_ik_session.cpp`.
9. Tests beside each area to confirm expected behaviour.

