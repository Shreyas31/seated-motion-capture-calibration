# OpenSim components

The OpenSim directory contains four related but distinct groups. They share
model and coordinate contracts, but they are separate programs rather than one
large OpenSim application.

## Model-generation utilities

`generate_seated_mocap_model` creates the patient-specific augmented Rajagopal
model. Supporting source files handle anthropometry, upper-body augmentation,
virtual IMU alignment, coordinate policy and support-plane landmarks.
`validate_seated_mocap_model` checks the resulting model.

## Pose-preset utilities

`generate_pose_presets` calculates the chair and bed pose data written to
`generated/pose_presets.json`. Before reporting success, it reloads both
written presets, applies them to the generated model, and verifies their model
reference orientations. This is an offline maintenance tool; it is not part of
the live viewer loop.

## Real-time viewer

`opensim_rt_viewer` receives the versioned UDP orientation packet, validates
and sequences frames, performs orientation IK, optionally applies the
stationary-feet translation estimate, renders the current state and writes
joint-angle CSV output.

Incoming packet validation and sequence-gap tracking are kept together in
`orientation_stream`. Joint-angle CSV output is private to
`realtime_ik_session`, which is its only consumer.

## Tests

Files under `tests/` cover OpenSim runtime initialisation, coordinate conversion,
segment/model ordering, viewer argument parsing, packet validation and frame
sequencing. They are registered with CTest and run as part of the
OpenSim-enabled preset.

## Dependencies

OpenSim uses the shared vendored Eigen and nlohmann/json libraries under the
project-root `dependencies` directory. Their versions, licences, and update
notes are documented in `dependencies/README.md`.
