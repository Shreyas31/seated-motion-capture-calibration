# Protocols and data contracts

This document describes compatibility boundaries between the Python frontend,
the C++ Xsens backend, and the C++ OpenSim viewer. Update producers, consumers,
tests, and this document together when a contract changes.

## Backend process command

The frontend launches:

```text
backend.exe --sensor-map <sensor_mapping.json>
```

The backend also accepts `-h` or `--help`. Unknown arguments and a missing value
for `--sensor-map` are fatal errors.

`SEATED_MOCAP_OUTPUT_DIR` is supplied in the backend environment so session
directories are created under the same data root used by the frontend.

## Frontend-to-backend input

Backend input is line-oriented UTF-8 text. A value is meaningful only while the
backend is waiting at the corresponding state. The frontend prevents actions
from writing into the wrong prompt.

| Backend state | Expected input | Meaning |
| --- | --- | --- |
| `SENSORS` | Empty line | Confirm the displayed sensor set. The frontend sends this automatically after all mapped sensors are connected. |
| `HEADING` | `y` or another character | `y`/`Y` resets offsets; any other character continues without changing them. This state is emitted only when the audit finds non-zero or unreadable offsets. |
| `SESSION` | One text line | Session/file prefix; the backend sanitises it before creating the directory. |
| `MENU` | `1` | Chair-seated static calibration. |
| `MENU` | `2` | Bed-seated static calibration. |
| `MENU` | `3` | Enter functional-calibration selection. |
| `MENU` | `4` | Start measurement recording. |
| `MENU` | `5` | Exit the backend. |
| `JOINT` | `1`–`8` | Select the functional movement in the order below. |
| `RECORDING` | `0` | Stop the active trial and close its outputs. |

Functional movement choices are:

1. Right knee.
2. Left knee.
3. Right elbow flexion/extension.
4. Right forearm pronation/supination.
5. Right shoulder abduction/adduction.
6. Left elbow flexion/extension.
7. Left forearm pronation/supination.
8. Left shoulder abduction/adduction.

The frontend starts a functional calibration in two writes: menu value `3`,
then the selected movement number after receiving `STATE|JOINT`.

## Backend-to-frontend line protocol

Structured backend records use:

```text
FRONTEND|<CATEGORY>|<NAME>[|<DETAIL>]
```

Rules:

- One record is terminated by a newline.
- Categories are `STATE`, `RESULT`, `ERROR`, or `GUIDANCE`.
- `NAME` is a stable uppercase identifier.
- `DETAIL` is optional and may contain additional `|` characters because the
  frontend splits the line into at most four fields.
- Ordinary console output may appear between structured records and is shown in
  Diagnostics but ignored by the parser.
- The frontend buffers incomplete lines delivered by `QProcess`.
- There is no escaping or length prefix. Do not place newlines in a detail.

### States

| Name | Backend is waiting for or doing |
| --- | --- |
| `SENSORS` | Sensor discovery and confirmation. |
| `HEADING` | Conditional heading-offset decision. |
| `SESSION` | Session name. |
| `MENU` | Main calibration/recording action. |
| `JOINT` | Functional movement selection. |
| `RECORDING` | Recording stop command. |
| `EXITING` | Normal process exit. |

### Results

| Name | Detail |
| --- | --- |
| `SENSOR_STATUS` | `<sensor-id>:CONNECTED` or `<sensor-id>:DISCONNECTED`. |
| `UNMAPPED_SENSOR` | Connected sensor ID not present in the mapping. |
| `SENSORS_CONNECTED` | Count of connected mapped sensors. |
| `SESSION_CONFIGURED` | Sanitised session name. |
| `STATIC_CALIBRATION` | Accepted pose label and summary produced by the workflow. |
| `FUNCTIONAL_CALIBRATION` | Accepted functional movement display name. |
| `RECORDING_STARTED` | Measurement CSV path. |
| `RECORDING_STOPPED` | Completed measurement CSV path. |

### Errors

| Name | Meaning |
| --- | --- |
| `SENSORS_INCOMPLETE` | Confirmation occurred while required mapped sensors were missing. |
| `STATIC_CALIBRATION` | Static capture or validation failed. |
| `FUNCTIONAL_CALIBRATION` | Functional capture, refinement, or return-pose validation failed. |
| `RECORDING` | Calibration was missing or CSV/UDP startup failed. |

Error details are clinician-facing diagnostics. Functional-calibration errors
may use `<movement-name>:<reason>` so the frontend can retain a failed status for
the selected movement.

### Guidance

Guidance names are:

- `STATIC_PREPARATION`
- `STATIC_CAPTURE`
- `STATIC_PROCESSING`
- `FUNCTIONAL_GRAVITY_PREPARATION`
- `FUNCTIONAL_GRAVITY_CAPTURE`
- `FUNCTIONAL_GRAVITY_PROCESSING`
- `FUNCTIONAL_MOVEMENT_PREPARATION`
- `FUNCTIONAL_MOVEMENT_CAPTURE`
- `FUNCTIONAL_RETURN_POSE_PREPARATION`
- `FUNCTIONAL_RETURN_POSE_CAPTURE`
- `FUNCTIONAL_PROCESSING`
- `COUNTDOWN`

`COUNTDOWN` details use `<phase>:<remaining-seconds>`. Functional capture events
that expose the backend-configured duration use
`<movement-name>|<seconds>`. The frontend remains compatible with older capture
events that contain only the movement name.

## Viewer command and line protocol

The frontend launches:

```text
opensim_rt_viewer <model.osim> <pose_presets.json> <pose-name> \
    <geometry-directory> [duration-seconds] \
    [stationary-feet|free-root] [joint-angle-output.csv]
```

The frontend currently supplies a positive duration, translation mode, and an
optional joint-angle output file. `stationary-feet` enables pelvis translation
estimation from planted feet; `free-root` leaves root translation unconstrained
by that feature.

Viewer records use:

```text
FRONTEND|VIEWER|<NAME>[|<DETAIL>]
```

| Name | Detail | Meaning |
| --- | --- | --- |
| `READY` | Pose name | Model and initial pose are loaded; the backend may begin streaming. |
| `STREAM_ENDED` | `INACTIVITY` | No valid frames were received for the inactivity timeout. |
| `CLOSED` | `USER` | The Simbody visualiser window was closed. |

The frontend waits for `READY` before sending menu command `4`, preventing the
initial UDP frames from being lost while OpenSim is still loading.

## UDP orientation protocol version 1

The backend sends packed binary datagrams to `127.0.0.1:9001`. The authoritative
definition is `shared/include/realtime_orientation_packet.h`.

Each version-1 datagram is 300 bytes:

| Field | Type | Bytes | Meaning |
| --- | --- | ---: | --- |
| `magic` | `char[4]` | 4 | ASCII `SMC1`. |
| `version` | `uint16` | 2 | Protocol version, currently `1`. |
| `sensorCount` | `uint16` | 2 | Must be `17`. |
| `sequence` | `uint64` | 8 | Monotonic frame sequence. |
| `timestampMicroseconds` | `int64` | 8 | Frame time in microseconds. |
| `validSensorMask` | `uint32` | 4 | One validity bit per ordered segment. |
| `orientations` | 17 × four `float` values | 272 | Scalar-first unit quaternions `(w,x,y,z)`. |

The protocol-fixed orientation order is:

```text
Pelvis, Sternum, Head,
Right_Shoulder, Right_Upperarm, Right_Forearm, Right_Hand,
Left_Shoulder, Left_Upperarm, Left_Forearm, Left_Hand,
Right_Upperleg, Right_Lowerleg, Right_Foot,
Left_Upperleg, Left_Lowerleg, Left_Foot
```

This order must remain identical to the OpenSim segment-model mapping. Never
derive packet positions from the order of entries in `sensor_mapping.json`.

The viewer rejects wrong magic, version, count, packet size, invalid masks,
non-finite quaternions, and unusable quaternion norms. It also tracks duplicate,
out-of-order, superseded, and missing sequence numbers.

## Sensor mapping JSON

`config/sensor_mapping.json` is a schema-version-1 document:

```json
{
  "schema_version": 1,
  "sensors": [
    {
      "segment": "Pelvis",
      "sensor_id": "physical-device-id"
    }
  ]
}
```

Validation requires:

- exactly the 17 canonical segment assignments;
- every segment recognised and assigned once;
- every sensor ID non-empty and assigned once.

The JSON array order is normalised when the frontend saves the mapping, but the
runtime UDP order is defined separately by the shared C++ contract.

## Patient anthropometry JSON

The frontend writes a version-1 JSON document for model generation. Fields are:

| Field | Type | Notes |
| --- | --- | --- |
| `subject_id` | string | Sanitised identifier used for paths. |
| `patient_height` | number | Metres. |
| `foot_length` | number or null | Metres. |
| `patient_mass_kg` | number or null | Optional. |
| `schema_version` | integer | Currently `1`. |
| `units` | string | Currently `m`. |

The file is written through a temporary file, flushed, and atomically replaced.

## CSV contracts

The authoritative fixed headers are in `shared/include/csv_schemas.h`.

### Measurement CSV

The backend writes long-format rows containing:

- packet ID, timestamp, and sensor ID;
- raw sensor-to-global quaternion `Q_Raw_GS`;
- static-calibrated body-to-session quaternion `Q_Static_CB`;
- final body-to-session quaternion `Q_Final_CB`;
- sensor-frame gyroscope in radians per second;
- sensor-frame acceleration in metres per second squared, including gravity;
- sensor-frame magnetic field in Xsens arbitrary units.

Quaternion components are scalar-first: `w,x,y,z`. Coordinate-frame suffixes
are part of the contract and should not be removed from column names.

### Relative-segment-orientation CSV

For each measurement file, the backend also writes a file ending in
`_RelativeSegmentOrientations.csv`. Its fixed prefix is
`PacketID,TimeMilliseconds`, followed by scalar-first `w,x,y,z` quaternion
columns for 17 named segment relationships. These include
`Session_to_Pelvis`, `Pelvis_to_Sternum`, and limb parent-to-child rotations.
The definitions and their order are in `backend/src/measurement_csv_writer.cpp`.
`TimeMilliseconds` comes from the Xsens packet arrival-time field; it is not an
independently synchronised optical timestamp.

Each relative rotation is calculated from the final calibrated segment
orientations as `inverse(parent) × child`; the pelvis is expressed in the
session frame. A row is written only after orientations for all 17 mapped
segments arrive with the same packet ID. Quaternion signs are chosen so the
scalar component is non-negative. These values describe segment relationships,
not OpenSim joint coordinates or independent optical measurements.

### Joint-angle CSV

The fixed prefix is:

```text
Sequence,TimestampMicroseconds,StreamTimeSeconds
```

Model-dependent OpenSim coordinate columns follow that prefix. The viewer writes
this file only when the optional output path is supplied.
`StreamTimeSeconds` is the viewer's stream time; it should not be treated as
the same clock as Vicon time without a separately estimated synchronisation.

### Calibration evidence

Static and functional workflows write additional CSV evidence through
`CalibrationCsvExporter`. These files belong to the session directory and are
used for audit and numerical diagnosis. Preserve their semantics when changing
calibration capture or acceptance logic.

## Session paths and naming

The default data root is:

```text
%USERPROFILE%\Documents\IRP_SeatedMoCap_Data
```

The backend sanitises the entered session name for filesystem use and creates a
session subdirectory. Measurement and joint-angle filenames use the next free
trial number so an existing trial is not overwritten.

The frontend stores patient model inputs and generated models beneath the same
data root. Logs are written under its `logs` subdirectory.

## Intentional Python/C++ duplication

The project deliberately keeps a few small contract representations in both
languages. This is easier to review than a schema compiler or generated-source
pipeline for a project of this size. The copies must be changed together and
are protected by `frontend/tests/test_cross_boundary_contracts.py`,
`test_frontend_protocol`, `test_data_contracts`, and
`test_functional_calibration_selection`.

| Contract | Python declaration | C++ declaration |
| --- | --- | --- |
| Backend event categories and names | `frontend/protocol.py` | `backend/include/frontend_protocol.h` |
| Functional movement numbers and labels | `frontend/protocol.py` | `backend/include/functional_calibration_types.h` vector order, interpreted as one-based choices |
| UDP version, byte size, and segment order | `frontend/protocol.py` | `shared/include/realtime_orientation_packet.h` |
| Measurement and joint-angle CSV headers | `frontend/protocol.py` | `shared/include/csv_schemas.h` |
| Session-name sanitisation examples | `frontend/tests/test_cross_boundary_contracts.py` | `backend/tests/test_data_contracts.cpp` |

`frontend/sensor_mapping.py::CANONICAL_SEGMENTS` controls the mapping editor's
display/save order. UDP array positions use `UDP_SEGMENT_ORDER` instead. Both
contain exactly the same 17 segments, but their order is intentionally allowed
to differ because the frontend never decodes the UDP packet.

Backend process events must be emitted with
`frontend_protocol::EventWriter`; backend workflows should not write raw
`FRONTEND|...` records. The OpenSim viewer has its own small
`FRONTEND|VIEWER|...` protocol because it is a separate executable that does
not link the backend library.

No source files are generated from these declarations.

## Compatibility checklist

Before changing a contract:

1. Identify every producer and consumer.
2. Decide whether the change requires a new protocol/schema version.
3. Update shared definitions instead of copying another literal.
4. Add or update contract tests in Python and C++.
5. Verify an actual frontend/backend/viewer session.
6. Preserve an older-data migration path when released files may still exist.

