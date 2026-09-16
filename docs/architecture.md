# Architecture

## Purpose and scope

The system collects calibrated orientations from 17 Xsens MTw sensors during a
seated clinical workflow. It separates clinician interaction, hardware
acquisition, and biomechanical visualisation into three processes so hardware
and OpenSim failures can be reported without freezing the UI.

The frontend is the composition root from the operator's perspective. It starts
the native programs, translates their machine-readable output into workflow
state, and sends values only when the backend is waiting at the matching prompt.

## Component overview

```text
+------------------------------------+
| PySide6 MainWindow                 |
|                                    |
| UI builders and action mixins      |
| WorkflowController -> SessionState |
| ProcessSupervisor                  |
|   |-- QProcess: backend -----------+---- line protocol over stdout
|   |-- QProcess: model tool --------+---- ordinary stdout/stderr
|   `-- QProcess: viewer ------------+---- viewer line protocol
+-----------------+------------------+
                  |
                  | stdin choices and session name
                  v
+------------------------+       UDP v1, loopback:9001
| Xsens backend          |--------------------------------+
|                        |                                |
| AwindaSystem           |                                v
| calibration workflows  |                     +---------------------+
| CSV writer             |                     | OpenSim viewer      |
| orientation streamer   |                     | real-time IK        |
+------------------------+                     | visualisation       |
                                               | joint-angle CSV     |
+------------------------+                     +---------------------+
| OpenSim model tool     |
| anthropometric scaling |
| model augmentation     |
+------------------------+
```

### Frontend

Important files:

- `frontend/app.py`: application composition, Qt construction, and entry point.
- `frontend/presentation_controller.py`: navigation, summaries, mapping, and
  preflight presentation.
- `frontend/execution_controller.py`: backend, model-generator, and viewer
  commands.
- `frontend/events_controller.py`: renders backend/viewer events and process
  results after non-visual transitions have been applied.
- `frontend/shutdown_controller.py`: ordered child-process shutdown.
- `frontend/runtime_context.py`: resolved paths and application logger.
- `frontend/app_config.py`: development and installed path resolution.
- `frontend/process_supervisor.py`: child-process ownership, environment setup,
  command construction, output buffering, protocol parsing, and termination
  fallback.
- `frontend/protocol.py`: parser for backend `FRONTEND|...` records.
- `frontend/session_state.py`: non-visual state for the active session.
- `frontend/workflow_controller.py`: backend-event state transitions and
  prerequisite checks that do not depend on widgets or page indices.
- `frontend/setup_page.py`: setup-page widgets and the sensor-mapping dialog.
- `frontend/calibration_page.py`: static and functional calibration widgets.
- `frontend/recording_page.py`: viewer preview and recording widgets.
- `frontend/workflow_ui.py`: diagnostics widgets and the shared workflow shell.

`ProcessSupervisor` owns all child-process lifetimes and buffers native output
because `QProcess` may deliver partial lines. `WorkflowController` applies
complete backend protocol records to `SessionState`. The mixins render those
changes, display ordinary output in diagnostic logs, and handle clinician
decisions and visible messages.

### Sensor backend

The backend is a C++20 console application. Its stdout contains both human
diagnostics and structured frontend records; stdin receives the response to the
current workflow prompt.

The high-level call sequence is:

```text
main
  -> Application::run
     -> ApplicationBootstrap::prepare
        -> load SensorMapping
        -> configure AwindaSystem
        -> wait for all mapped sensors
        -> audit/reset heading offsets
        -> enter measurement mode
        -> prepare SessionContext
     -> AcquisitionWorkflow::run
        -> ApplicationMenu::collect
        -> static calibration, functional calibration, recording, or exit
```

`ApplicationBootstrapResult` owns the mapping, session context, and
`AwindaSystem` for the acquisition lifetime. In `AcquisitionWorkflow`, callback
registration is deliberately declared after the packet collector so the
callback is removed before its target is destroyed during normal return or
exception unwinding.

### OpenSim programs

The OpenSim directory contains several executables rather than one library:

- `generate_seated_mocap_model`: scales and augments the Rajagopal source model
  using patient anthropometry.
- `generate_pose_presets`: generates the supported chair/bed initial poses,
  reloads the written file, and verifies both presets against the model.
- `validate_seated_mocap_model`: validates generated model structure.
- `opensim_rt_viewer`: receives live orientations, runs IK, updates the Simbody
  visualiser, and optionally writes joint angles.
- automated runtime and protocol-oriented test executables.

The runtime viewer uses a versioned binary UDP contract from
`shared/include/realtime_orientation_packet.h`. It does not read the backend's
measurement CSV during live operation.

## Application lifecycle

### 1. Frontend startup

`resolve_app_paths()` chooses a development or installed layout. `MainWindow`
creates one `ProcessSupervisor`, which owns the backend, model-generator, and
viewer `QProcess` objects, and one `WorkflowController`, which owns the active
`SessionState`. The frontend loads the existing light/dark stylesheet, reads the
sensor mapping, and leaves all hardware actions disabled until their
prerequisites are satisfied.

### 2. Patient model preparation

The frontend writes a version-1 anthropometry JSON document beneath the session
data root and launches `generate_seated_mocap_model`. The generator receives:

1. Rajagopal source model.
2. Output model path.
3. source geometry directory.
4. anthropometry JSON path.

Changing session name, height, or foot length invalidates the prepared model and
requires regeneration.

### 3. Backend bootstrap

The frontend launches `backend.exe --sensor-map <mapping.json>` and supplies
`SEATED_MOCAP_OUTPUT_DIR` to the process environment.

The backend then:

1. Validates the complete, unique 17-segment mapping.
2. Configures the Awinda update rate and radio channel.
3. Polls connection state and reports individual sensors.
4. Continues after the frontend confirms all mapped sensors.
5. Audits device heading offsets while the master is still in configuration
   mode and optionally resets them.
6. Enters measurement mode.
7. Reads and sanitises the session name.
8. Creates the session output directory.

Heading offsets cannot be changed safely after acquisition mode begins, which
is why heading resolution occurs before session recording actions.

### 4. Static calibration

Static calibration is required before functional calibration or recording.

The workflow:

1. Selects chair or bed pose targets.
2. Runs a preparation countdown.
3. Buffers a stationary capture from every mapped sensor.
4. Estimates the session-relative horizontal frame.
5. Estimates average sensor orientations, gyroscope biases, and gravity
   directions with uncertainty.
6. Rejects incomplete or poor-quality sensor data.
7. Commits all sensor offsets atomically only when the complete calibration is
   valid.
8. Updates the measurement writer and exports calibration evidence.

The core numerical implementation is in `seated_calibration`, while capture and
commit sequencing is in `static_calibration_workflow` and
`calibration_session`.

### 5. Functional calibration

Functional calibration is optional and refines the accepted static offsets for
a selected proximal/distal sensor pair.

The workflow is intentionally staged:

```text
select joint
  -> resolve calibrated sensor pair
  -> capture stationary gravity
  -> capture repeated movement
  -> estimate functional axes
  -> build vector observations
  -> solve candidate offsets
  -> return to static reference pose
  -> validate candidate against the reference
  -> commit both sides together
  -> export evidence
```

Rejected repeats do not overwrite previously accepted offsets. Observation
sources are replaced deliberately when the same functional movement is repeated.

The functional area is grouped by responsibility rather than by individual
helper function: `functional_calibration_types` defines the supported movements,
`functional_calibration_capture` owns data acquisition,
`functional_calibration_solver` contains axis analysis and refinement,
`functional_return_pose` validates the independent return capture, and
`functional_calibration_workflow` owns selection, sequencing, and final export.

### 6. Recording and live visualisation

Before recording, the frontend starts the viewer with the generated model,
selected pose preset, OpenSim geometry directory, translation mode, and a
joint-angle output path. It waits for `FRONTEND|VIEWER|READY` before sending the
backend recording command.

The backend starts the UDP streamer before opening the measurement CSV. During
recording, the packet collector supplies calibrated orientations to both the CSV
writer and UDP streamer. The backend writes a per-sensor measurement CSV and a
per-frame relative-segment-orientation CSV. The viewer separately writes
OpenSim joint angles when given an output path. Entering `0` stops the trial,
closes both backend CSVs, stops streaming, and preserves calibration for another
trial.

The viewer validates each packet, tracks sequence gaps, runs IK, updates the
visualiser at a bounded rate, and ends after stream inactivity or user closure.

### 7. Shutdown

The frontend performs ordered shutdown:

1. Stop active recording so CSV data can be flushed.
2. Stop the viewer.
3. Stop unfinished model generation.
4. Ask a backend at the menu to exit normally, or terminate an uninterruptible
   prompt after clinician confirmation.
5. Kill remaining child processes only after the shutdown timeout.

Changes to process ownership or signals must preserve this order.

## Runtime paths

`frontend/app_config.py` resolves these environment overrides:

| Variable | Meaning | Development default |
| --- | --- | --- |
| `SEATED_MOCAP_SENSOR_MAP` | Sensor mapping JSON | `config/sensor_mapping.json` |
| `SEATED_MOCAP_OUTPUT_DIR` | Session data, models, and logs | `%USERPROFILE%/Documents/IRP_SeatedMoCap_Data` |
| `RAJAGOPAL_SOURCE_MODEL` | Source model for patient generation | OpenSense example model, unless bundled |
| `OPENSIM_HOME` | OpenSim runtime root | Bundled runtime when installed; otherwise required for development |

Installed builds resolve native executables from `bin/`, resources from
`resources/`, and OpenSim from `runtime/OpenSim/`. Development mode resolves the
native programs from `out/build/x64-opensim-relwithdebinfo`.

## Source ownership and boundaries

The following are compatibility boundaries:

- `config/sensor_mapping.json`: physical device assignment.
- `frontend/protocol.py` and `backend/frontend_protocol`: line protocol.
- `shared/include/realtime_orientation_packet.h`: UDP packet version and order.
- `shared/include/csv_schemas.h`: stable CSV prefixes and measurement header.
- `opensim/include/coordinate_conventions.h`: session-to-OpenSim coordinate
  transformation.
- `opensim/include/segment_model_map.h`: protocol segment to OpenSim component
  mapping.

Changes to a boundary require tests on both sides. Human-readable console text
is diagnostic and may evolve; structured tokens and column order are contracts.

## Recommended code-reading path

For a new developer:

1. Read `frontend/app_config.py`, `process_supervisor.py`, and `protocol.py` to
   understand runtime and process contracts.
2. Read `session_state.py` and `workflow_controller.py` for non-visual state,
   backend-event transitions, and action prerequisites.
3. Read `setup_page.py`, `calibration_page.py`, `recording_page.py`, and
   `workflow_ui.py` for the page-oriented UI structure.
4. Read `frontend/app.py` for composition and signal wiring, then read
   `presentation_controller.py`, `execution_controller.py`,
   `events_controller.py`, and `shutdown_controller.py` in that order.
5. Follow backend control flow from `main.cpp` through `application`,
   `application_bootstrap`, and `acquisition_workflow`.
6. Read the line, UDP, CSV, coordinate, and segment mapping contracts.
7. Read static calibration before functional calibration.
8. Follow recording from `packet_collector` to `measurement_csv_writer` and
   `udp_orientation_streamer`.
9. Follow viewer input from `udp_orientation_receiver` to
   `realtime_ik_session` and `realtime_viewer_runner`.
10. Read tests beside each responsibility to see accepted inputs and failure
    behaviour.

