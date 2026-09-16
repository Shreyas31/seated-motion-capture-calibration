# Vendored dependencies

This directory is the single location for third-party source, headers,
libraries, and runtime binaries committed with the project. Application
components consume these dependencies through CMake targets or component-local
build settings; they must not reach into another component's source tree.

## Eigen

- **Path:** `Eigen/`
- **Version:** 5.0.1
- **Purpose:** Linear algebra and quaternion calculations used by the backend
  and the OpenSim pose-preset generator.
- **Source:** <https://eigen.tuxfamily.org/>
- **Licence:** Mozilla Public License 2.0, with additional licences applying to
  individual upstream files. The upstream `COPYING.*` and `LICENSE` files are
  retained beside the headers.

## JSON for Modern C++

- **Path:** `nlohmann/json.hpp`
- **Version:** 3.7.3
- **Purpose:** Reading sensor mappings, patient anthropometry, and OpenSim pose
  presets.
- **Source:** <https://github.com/nlohmann/json>
- **Licence:** MIT; the upstream copyright and licence notice are retained in
  the single-header distribution.

Replace the complete upstream header when updating this dependency. Do not
reformat it, and update the version recorded here.

## Xsens Device API

- **Path:** `xsens/`
- **Purpose:** Headers, import libraries, and runtime DLLs used to communicate
  with Xsens hardware.
- **Source:** The Movella/Xsens MT Software Suite installed for this project.
- **Licence:** Movella/Xsens and bundled third-party terms. See
  `xsens/lib/license.txt` and the notices retained in the SDK headers.

The Xsens SDK is consumed only by the backend, but it lives here so every
vendored dependency has one predictable location. The repository owner has
confirmed that redistribution is permitted under the applicable agreement.
Retain all SDK copyright, licence, and third-party notices when publishing the
repository or distributing the application.

## External prerequisites

The OpenSim SDK and Rajagopal source model are deliberately not vendored here.
Configure them with `OPENSIM_HOME` and `RAJAGOPAL_SOURCE_MODEL` as described in
the root README. Python packages are installed into a local virtual environment
from `frontend/requirements.txt`.
