from PySide6.QtWidgets import QDialog

from frontend.sensor_mapping import (
    CANONICAL_SEGMENTS,
    EXPECTED_SENSOR_COUNT,
    load_sensor_mapping,
)
from frontend.setup_page import SensorMappingDialog


def valid_mapping():
    return {
        segment: f"SENSOR-{index:02d}"
        for index, segment in enumerate(CANONICAL_SEGMENTS, start=1)
    }


def test_complete_mapping_enables_save(qtbot, tmp_path):
    dialog = SensorMappingDialog(valid_mapping(), tmp_path / "mapping.json")
    qtbot.addWidget(dialog)

    assert dialog.validate_table()
    assert dialog.save_button.isEnabled()
    assert f"{EXPECTED_SENSOR_COUNT} unique" in dialog.validation_label.text()


def test_duplicate_sensor_id_disables_save(qtbot, tmp_path):
    dialog = SensorMappingDialog(valid_mapping(), tmp_path / "mapping.json")
    qtbot.addWidget(dialog)
    dialog.table.item(1, 1).setText(dialog.table.item(0, 1).text())

    assert not dialog.validate_table()
    assert not dialog.save_button.isEnabled()
    assert "Duplicate IDs" in dialog.validation_label.text()


def test_save_normalises_ids_and_accepts_dialog(qtbot, tmp_path):
    mapping = {
        segment: f" sensor-{index:02d} "
        for index, segment in enumerate(CANONICAL_SEGMENTS, start=1)
    }
    output_path = tmp_path / "mapping.json"
    dialog = SensorMappingDialog(mapping, output_path)
    qtbot.addWidget(dialog)

    dialog.save_and_accept()

    assert dialog.result() == QDialog.DialogCode.Accepted
    assert load_sensor_mapping(output_path) == valid_mapping()


def test_sensor_id_text_is_black(
    qtbot,
    tmp_path,
):
    dialog = SensorMappingDialog(
        valid_mapping(),
        tmp_path / "sensor_mapping.json",
    )
    qtbot.addWidget(dialog)

    for row in range(len(CANONICAL_SEGMENTS)):
        sensor_item = dialog.table.item(
            row,
            1,
        )

        assert sensor_item is not None
        assert sensor_item.foreground().color().name().lower() == "#000000"
