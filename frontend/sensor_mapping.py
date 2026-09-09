import json
import os
from collections.abc import Mapping
from pathlib import Path
from typing import Any

CANONICAL_SEGMENTS = (
    "Right_Hand",
    "Left_Hand",
    "Right_Forearm",
    "Left_Forearm",
    "Right_Upperarm",
    "Left_Upperarm",
    "Right_Shoulder",
    "Left_Shoulder",
    "Right_Foot",
    "Left_Foot",
    "Right_Lowerleg",
    "Left_Lowerleg",
    "Right_Upperleg",
    "Left_Upperleg",
    "Pelvis",
    "Sternum",
    "Head",
)

EXPECTED_SENSOR_COUNT = len(CANONICAL_SEGMENTS)


def parse_sensor_mapping(document: Any) -> dict[str, str]:
    """Validate a schema-version-1 document and return segment assignments."""

    if not isinstance(document, dict):
        raise ValueError("Sensor mapping root must be a JSON object.")

    if document.get("schema_version") != 1:
        raise ValueError("Sensor mapping must use schema_version 1.")

    entries = document.get("sensors")

    if not isinstance(entries, list):
        raise ValueError("Sensor mapping must contain a sensors array.")

    recognised_segments = set(CANONICAL_SEGMENTS)
    mapping: dict[str, str] = {}
    assigned_sensor_ids: set[str] = set()

    for index, entry in enumerate(entries):
        if not isinstance(entry, dict):
            raise ValueError(f"Sensor mapping entry {index + 1} is not an object.")

        segment = entry.get("segment")
        sensor_id = entry.get("sensor_id")

        if not isinstance(segment, str) or not segment.strip():
            raise ValueError("Every mapping entry requires a non-empty segment.")

        if not isinstance(sensor_id, str) or not sensor_id.strip():
            raise ValueError(f"{segment} requires a non-empty sensor_id.")

        segment = segment.strip()
        sensor_id = sensor_id.strip()

        if segment not in recognised_segments:
            raise ValueError(f"Unknown segment in sensor mapping: {segment}")

        if segment in mapping:
            raise ValueError(f"Segment is assigned more than once: {segment}")

        if sensor_id in assigned_sensor_ids:
            raise ValueError(f"Sensor ID is assigned more than once: {sensor_id}")

        mapping[segment] = sensor_id
        assigned_sensor_ids.add(sensor_id)

    missing_segments = [
        segment for segment in CANONICAL_SEGMENTS if segment not in mapping
    ]

    if missing_segments:
        raise ValueError(
            "Sensor mapping is incomplete. Missing:\n" + ", ".join(missing_segments)
        )

    if len(mapping) != len(CANONICAL_SEGMENTS):
        raise ValueError(
            "Sensor mapping must contain exactly "
            f"{len(CANONICAL_SEGMENTS)} segment assignments."
        )

    return mapping


def load_sensor_mapping(path: Path) -> dict[str, str]:
    try:
        with Path(path).open("r", encoding="utf-8") as input_file:
            document = json.load(input_file)
    except FileNotFoundError as error:
        raise ValueError(f"Sensor mapping file not found:\n{path}") from error
    except json.JSONDecodeError as error:
        raise ValueError(f"Sensor mapping contains invalid JSON:\n{error}") from error
    except OSError as error:
        raise ValueError(f"Could not read sensor mapping:\n{error}") from error

    return parse_sensor_mapping(document)


def save_sensor_mapping(path: Path, mapping: Mapping[str, str]) -> None:
    entries = [
        {"segment": segment, "sensor_id": sensor_id}
        for segment, sensor_id in mapping.items()
    ]
    validated_mapping = parse_sensor_mapping({"schema_version": 1, "sensors": entries})
    document = {
        "schema_version": 1,
        "sensors": [
            {
                "segment": segment,
                "sensor_id": validated_mapping[segment],
            }
            for segment in CANONICAL_SEGMENTS
        ],
    }

    output_path = Path(path)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    temporary_path = output_path.with_suffix(output_path.suffix + ".tmp")

    try:
        with temporary_path.open("w", encoding="utf-8") as output_file:
            json.dump(document, output_file, indent=2)
            output_file.write("\n")
            output_file.flush()
            os.fsync(output_file.fileno())

        os.replace(temporary_path, output_path)
    except OSError as error:
        try:
            temporary_path.unlink(missing_ok=True)
        except OSError:
            pass

        raise ValueError(f"Could not save sensor mapping:\n{error}") from error
