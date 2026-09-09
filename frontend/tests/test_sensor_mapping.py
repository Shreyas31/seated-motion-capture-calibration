import json

import pytest

from frontend.sensor_mapping import (
    CANONICAL_SEGMENTS,
    load_sensor_mapping,
    parse_sensor_mapping,
    save_sensor_mapping,
)


def valid_mapping():
    return {
        segment: f"sensor-{index:02d}"
        for index, segment in enumerate(CANONICAL_SEGMENTS, start=1)
    }


def mapping_document(mapping):
    return {
        "schema_version": 1,
        "sensors": [
            {"segment": segment, "sensor_id": sensor_id}
            for segment, sensor_id in mapping.items()
        ],
    }


def test_parse_accepts_complete_unique_mapping():
    mapping = valid_mapping()

    assert parse_sensor_mapping(mapping_document(mapping)) == mapping


def test_parse_rejects_duplicate_sensor_ids():
    mapping = valid_mapping()
    mapping["Head"] = mapping["Pelvis"]

    with pytest.raises(ValueError, match="assigned more than once"):
        parse_sensor_mapping(mapping_document(mapping))


def test_parse_rejects_missing_segment():
    mapping = valid_mapping()
    del mapping["Head"]

    with pytest.raises(ValueError, match="Missing:\\nHead"):
        parse_sensor_mapping(mapping_document(mapping))


def test_load_reports_invalid_json(tmp_path):
    mapping_path = tmp_path / "mapping.json"
    mapping_path.write_text("{invalid", encoding="utf-8")

    with pytest.raises(ValueError, match="invalid JSON"):
        load_sensor_mapping(mapping_path)


def test_save_and_load_round_trip_in_canonical_order(tmp_path):
    mapping_path = tmp_path / "config" / "mapping.json"
    reversed_mapping = dict(reversed(tuple(valid_mapping().items())))

    save_sensor_mapping(mapping_path, reversed_mapping)

    assert load_sensor_mapping(mapping_path) == valid_mapping()
    document = json.loads(mapping_path.read_text(encoding="utf-8"))
    assert [entry["segment"] for entry in document["sensors"]] == list(
        CANONICAL_SEGMENTS
    )
    assert not mapping_path.with_suffix(".json.tmp").exists()
