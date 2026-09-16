# Testing and validation

Testing is divided into hardware-free automated checks, native OpenSim checks,
manual Xsens validation, and release validation. A change is not complete merely
because the frontend starts.

## Test environments

### Python environment

From the repository root:

```powershell
py -3.11 -m venv .venv
.\.venv\Scripts\python.exe -m pip install --upgrade pip
.\.venv\Scripts\python.exe -m pip install -r frontend\requirements-dev.txt
```

### Native environment

Run CMake from a Visual Studio developer PowerShell so `cl.exe` and the selected
generator/toolset are available. For OpenSim builds:

```powershell
$env:OPENSIM_HOME = "C:\OpenSim 4.5"
```

Use the actual installation path when it differs.

## Fast local checks

Run these after an ordinary frontend or pure-logic change:

```powershell
.\.venv\Scripts\python.exe -m ruff check frontend
.\.venv\Scripts\python.exe -m pytest
```

Pytest currently collects 132 tests. They cover configuration, session state,
protocol parsing, command construction, file naming and atomic writes, sensor
mapping, UI construction and visible contracts, workflow-controller
transitions, process supervision, cross-boundary Python/C++ contracts, process
results, and safe shutdown behaviour.

The tests use temporary directories and simulated process/state transitions;
they do not connect to Awinda hardware.

For headless environments, set Qt's offscreen platform before pytest:

```powershell
$env:QT_QPA_PLATFORM = "offscreen"
.\.venv\Scripts\python.exe -m pytest
```

Do not use offscreen mode for the final visual review.

## Python test commands

Run one module:

```powershell
.\.venv\Scripts\python.exe -m pytest frontend\tests\test_frontend_state.py
```

Run one test:

```powershell
.\.venv\Scripts\python.exe -m pytest `
    frontend\tests\test_frontend_state.py::test_recording_state
```

Run without pytest's cache when validating a clean repository:

```powershell
.\.venv\Scripts\python.exe -m pytest -p no:cacheprovider
```

Check formatting without rewriting files:

```powershell
.\.venv\Scripts\python.exe -m ruff format --check frontend
```

`ruff check` and `ruff format --check` validate different concerns; run both
before completing a formatting or import cleanup.

## Backend-only native tests

Configure and build:

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug
```

Then run:

```powershell
ctest --preset x64-debug
```

If CTest reports `No tests were found`, reconfigure a fresh build directory and
confirm that `BUILD_TESTING` is enabled. An old build directory may predate the
current test declarations or contain a cached `BUILD_TESTING=OFF` value.

Backend tests cover calibration mathematics, uncertainty, session commits,
protocol formatting, path configuration, menu parsing, sensor status analysis,
functional capture prerequisites, functional targets/observations, return-pose
decisions, and recording-stop input.

Some tests link the vendored Xsens libraries but still use synthetic data and do
not require a connected Awinda system.

## Full native and OpenSim tests

Configure and build:

```powershell
$env:OPENSIM_HOME = "C:\OpenSim 4.5"
cmake --preset x64-opensim-relwithdebinfo
cmake --build --preset x64-opensim-relwithdebinfo
```

Run all registered C++ tests:

```powershell
ctest --preset x64-opensim-relwithdebinfo
```

The current OpenSim-enabled preset registers 24 tests. OpenSim-oriented tests
cover runtime initialisation, coordinate conventions, segment-model mapping,
viewer argument parsing, UDP orientation validation, and frame-sequence
tracking.
The viewer-configuration, orientation-validation, and frame-sequence tests use
explicit failure checks, so their conditions remain active in RelWithDebInfo
builds where C/C++ `assert()` may be disabled.

Run a specific CTest by name:

```powershell
ctest --preset x64-opensim-relwithdebinfo `
    -R realtime_orientation_validation
```

Run the focused OpenSim runtime test when diagnosing SDK discovery or runtime
DLL problems:

```powershell
ctest --preset x64-opensim-relwithdebinfo -R opensim_runtime
```

## Unified local check

`scripts/check.ps1` performs the normal local validation sequence: Ruff lint
and formatting, clang-format verification, pytest, CMake configure, native
build, and CTest. It excludes bundled SDK and third-party headers from
formatting checks and disables Ruff and pytest caches so the check does not add
local cache files to the repository.

Run the backend-only validation:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\check.ps1 `
    -Preset x64-debug
```

Run the full OpenSim validation:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\check.ps1 `
    -Preset x64-opensim-relwithdebinfo
```

When a Python environment has not yet been created, `-SkipPython` runs only the
native formatting/configure/build/test stages. `-SkipFormat` is available for a
build-only check on machines without the Visual Studio C++ Clang tools.
Omitting both switches is the normal pre-handoff check.

### Formatting first-party code

Format Python code with the project environment:

```powershell
.venv\Scripts\python -m ruff format frontend
```

Format changed C++ files with the Visual Studio `clang-format` executable or
the IDE's clang-format integration. Apply `.clang-format` only to first-party
files under `backend`, `opensim`, and `shared`; do not reformat anything under
the root `dependencies` directory.

Header documentation is part of the public interface. Retain Doxygen
descriptions for classes and functions, document every parameter with
`@param`, and document non-void results with `@return`. The clang-format policy
disables comment reflow so these descriptions are not rewritten mechanically.

## What automated tests do not prove

Automated tests do not validate:

- radio discovery and connectivity changes;
- physical sensor identifiers;
- Xsens heading-offset persistence;
- sensor mounting on a participant;
- clinical pose reproducibility;
- real-time visual latency;
- clean CSV shutdown during real acquisition;
- installer behaviour on a machine without development tools.

These require the checks below.

## Optical pilot evaluation

The thesis evaluation is separate from the automated software tests. Two lab
participants completed chair- and bed-seated validation recordings with 17
Xsens IMUs and Vicon optical markers. A companion
`validation_reproducibility_package`, kept outside this application source
repository, preserves the selected recordings, the models and scripts used for
optical marker IK, synchronisation decisions, analysis windows and cycles,
quality-control outputs, and the reported agreement tables and waveforms.

The frozen primary analysis includes seven dynamic records, 28 included task
windows and 140 selected cycles. It compares IMU OpenSim joint-angle CSVs with
optical marker IK angles from the corresponding participant-specific model.
The measurement and relative-segment-orientation CSVs generated during those
sessions are retained as supporting exports; they were not used as substitute
optical reference angles. Two additional P01 chair export pairs were archived
but were not part of the reported optical comparison.

This small technical pilot assesses feasibility and exposes task-specific
errors. It does not establish population-level equivalence or patient-use
accuracy. Marker occlusion and optical model-fit quality limit some intervals,
so the package records exclusions and QC rather than treating every completed
IK frame as valid reference data. Consult the package README for the rerun
commands. Review participant consent, lab sharing terms and third-party model
licences before publishing its data.

## Manual hardware validation

Use `frontend/tests/MANUAL_HARDWARE_TESTS.md` as the authoritative checklist.
Record the date, software revision, operator, hardware set, and outcome.

At minimum, a release candidate must validate:

1. System Check and understandable missing-dependency errors.
2. Sensor count from 0 through all 17 mapped sensors.
3. Rejection of incomplete sensor sets.
4. Heading-offset audit/reset when applicable.
5. Patient model generation and invalidation after input changes.
6. Chair static calibration and, where supported, bed static calibration.
7. Successful and rejected functional calibrations.
8. Viewer readiness before recording starts.
9. Measurement and joint-angle CSV creation.
10. Normal recording stop with readable, non-empty files.
11. A second trial without recalibration.
12. Backend/viewer failure reporting.
13. Safe application shutdown and useful logs.

Use anonymised session identifiers during development testing.

## Frontend visual regression check

Automated Qt tests protect much of the structure and state, but they are not a
pixel-level guarantee. When changing frontend implementation:

- compare the application at its default 1000 x 700 size;
- inspect light and dark system colour schemes;
- visit Setup, Calibration, Recording, and Diagnostics;
- compare the sensor table collapsed and expanded;
- exercise disabled, ready, busy, success, error, and recording states;
- confirm dialog wording, default buttons, and tab order;
- test Windows display scaling used on the acquisition computer.

Do not accept visual differences as incidental to a non-visual refactor.

## Data-contract regression checks

For changes near recording, calibration export, or streaming:

1. Compare the measurement CSV header with
   `shared/include/csv_schemas.h`.
2. Confirm quaternion column order remains scalar-first.
3. Confirm units and coordinate-frame suffixes remain unchanged.
4. Verify measurement and joint-angle trial numbers match.
5. Verify the UDP frame is still version 1, 300 bytes, and ordered according
   to `kSegmentOrder`.
6. Confirm viewer statistics show no unexplained gaps or IK failures under
   normal use.
7. Confirm a failed or repeated functional calibration preserves the last
   accepted offsets as designed.

## Release validation

Create the portable application:

```powershell
powershell -ExecutionPolicy Bypass -File deployment\build_release.ps1
```

If paths are non-standard:

```powershell
powershell -ExecutionPolicy Bypass -File deployment\build_release.ps1 `
    -OpenSimHome "C:\OpenSim 4.5" `
    -RajagopalSourceModel "C:\path\to\Rajagopal_2015.osim"
```

After installing Inno Setup 6, create the installer from the validated release:

```powershell
powershell -ExecutionPolicy Bypass -File deployment\build_installer.ps1 `
    -SkipReleaseBuild
```

Validate the installer on a clean x64 Windows computer or VM without Python or
OpenSim installed. The packaged application must supply Python, PySide6,
OpenSim, pose presets, and the Rajagopal source model. The Xsens Windows driver
must still be installed separately.

On the clean machine:

1. Install and start from the Start menu.
2. Run System Check.
3. Generate a patient model.
4. Connect Awinda hardware.
5. Complete static calibration.
6. Start the viewer and record a trial.
7. Inspect measurement and joint-angle CSV files.
8. Review the frontend log.
9. Confirm data is written beneath `Documents\IRP_SeatedMoCap_Data`, not the
   installation directory.

Review third-party redistribution terms before distributing any installer.

## Completion gate for a change

A change is ready to merge or hand off when:

- the relevant focused tests pass;
- the full Python suite passes;
- applicable native tests pass;
- Ruff checks pass;
- protocol/CSV tests are updated for intentional contract changes;
- hardware tests are completed when hardware behaviour changed;
- visual review is completed when frontend code changed;
- release assembly is tested when paths, packaging, or dependencies changed;
- no participant data, logs, caches, binaries, or generated patient models were
  added to source control.

