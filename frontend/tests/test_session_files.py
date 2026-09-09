import json

import pytest

from frontend.session_files import (
    PatientAnthropometry,
    next_joint_angle_csv_path,
    patient_model_files,
    safe_session_name,
    safe_subject_id,
    session_data_directory,
    write_patient_anthropometry,
)


def test_safe_names_are_filesystem_friendly():
    assert safe_session_name(" Patient 08 / chair ") == "Patient_08_chair"
    assert safe_session_name("///") == "Session"
    assert safe_subject_id(" Patient.08 / chair ") == "Patient.08_chair"


def test_subject_id_must_contain_usable_characters():
    with pytest.raises(ValueError, match="no usable characters"):
        safe_subject_id("...")


def test_joint_angle_path_skips_existing_measurement_trials(tmp_path):
    session_directory = tmp_path / "P001"
    session_directory.mkdir()
    (session_directory / "P001_Measurement_1.csv").touch()
    (session_directory / "P001_Measurement_2_JointAngles.csv").touch()

    assert next_joint_angle_csv_path(tmp_path, "P001") == (
        session_directory / "P001_Measurement_3_JointAngles.csv"
    )


def test_patient_model_paths_use_safe_subject_directory(tmp_path):
    files = patient_model_files(tmp_path, "Patient 01")

    assert files.directory == tmp_path / "Patient_01" / "model"
    assert files.anthropometry_file == (files.directory / "patient_anthropometry.json")
    assert files.model_file == files.directory / "Rajagopal_SeatedMoCap.osim"


def test_session_data_directory_uses_sanitised_name(tmp_path):
    assert session_data_directory(tmp_path, "Patient 01 / Chair") == (
        tmp_path / "Patient_01_Chair"
    )


def test_anthropometry_round_trip_and_atomic_cleanup(tmp_path):
    output_path = tmp_path / "patient" / "patient_anthropometry.json"
    anthropometry = PatientAnthropometry(
        subject_id="P001",
        patient_height=1.72,
        foot_length=0.25,
    )

    write_patient_anthropometry(output_path, anthropometry)

    assert json.loads(output_path.read_text(encoding="utf-8")) == {
        "subject_id": "P001",
        "patient_height": 1.72,
        "foot_length": 0.25,
        "patient_mass_kg": None,
        "schema_version": 1,
        "units": "m",
    }
    assert not output_path.with_suffix(".json.tmp").exists()
