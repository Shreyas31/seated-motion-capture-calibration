import pytest

from frontend.protocol import (
    ERROR_NAMES,
    FUNCTIONAL_CALIBRATIONS,
    GUIDANCE_NAMES,
    MEASUREMENT_CSV_HEADER,
    OPEN_SIM_JOINT_ANGLE_CSV_PREFIX,
    PROTOCOL_CATEGORIES,
    RESULT_NAMES,
    STATE_NAMES,
    UDP_PACKET_SIZE_BYTES,
    UDP_PROTOCOL_VERSION,
    UDP_SEGMENT_ORDER,
)
from frontend.sensor_mapping import CANONICAL_SEGMENTS, EXPECTED_SENSOR_COUNT
from frontend.session_files import safe_session_name


def test_backend_protocol_vocabulary_matches_cpp_contract():
    assert PROTOCOL_CATEGORIES == ("STATE", "RESULT", "ERROR", "GUIDANCE")
    assert STATE_NAMES == (
        "SENSORS",
        "HEADING",
        "SESSION",
        "MENU",
        "JOINT",
        "RECORDING",
        "EXITING",
    )
    assert RESULT_NAMES == (
        "SENSOR_STATUS",
        "UNMAPPED_SENSOR",
        "SENSORS_CONNECTED",
        "SESSION_CONFIGURED",
        "STATIC_CALIBRATION",
        "FUNCTIONAL_CALIBRATION",
        "RECORDING_STARTED",
        "RECORDING_STOPPED",
    )
    assert ERROR_NAMES == (
        "SENSORS_INCOMPLETE",
        "STATIC_CALIBRATION",
        "FUNCTIONAL_CALIBRATION",
        "RECORDING",
    )
    assert GUIDANCE_NAMES == (
        "STATIC_PREPARATION",
        "STATIC_CAPTURE",
        "STATIC_PROCESSING",
        "FUNCTIONAL_GRAVITY_PREPARATION",
        "FUNCTIONAL_GRAVITY_CAPTURE",
        "FUNCTIONAL_GRAVITY_PROCESSING",
        "FUNCTIONAL_MOVEMENT_PREPARATION",
        "FUNCTIONAL_MOVEMENT_CAPTURE",
        "FUNCTIONAL_RETURN_POSE_PREPARATION",
        "FUNCTIONAL_RETURN_POSE_CAPTURE",
        "FUNCTIONAL_PROCESSING",
        "COUNTDOWN",
    )


def test_functional_calibration_choices_match_cpp_order():
    assert FUNCTIONAL_CALIBRATIONS == (
        ("Right knee", 1),
        ("Left knee", 2),
        ("Right elbow flexion/extension", 3),
        ("Right forearm pronation/supination", 4),
        ("Right shoulder abduction/adduction", 5),
        ("Left elbow flexion/extension", 6),
        ("Left forearm pronation/supination", 7),
        ("Left shoulder abduction/adduction", 8),
    )


def test_udp_contract_matches_shared_cpp_packet():
    assert UDP_PROTOCOL_VERSION == 1
    assert UDP_PACKET_SIZE_BYTES == 300
    assert len(UDP_SEGMENT_ORDER) == EXPECTED_SENSOR_COUNT == 17
    assert set(UDP_SEGMENT_ORDER) == set(CANONICAL_SEGMENTS)
    assert UDP_SEGMENT_ORDER == (
        "Pelvis",
        "Sternum",
        "Head",
        "Right_Shoulder",
        "Right_Upperarm",
        "Right_Forearm",
        "Right_Hand",
        "Left_Shoulder",
        "Left_Upperarm",
        "Left_Forearm",
        "Left_Hand",
        "Right_Upperleg",
        "Right_Lowerleg",
        "Right_Foot",
        "Left_Upperleg",
        "Left_Lowerleg",
        "Left_Foot",
    )


def test_csv_headers_match_shared_cpp_schemas():
    assert MEASUREMENT_CSV_HEADER.split(",") == [
        "PacketID",
        "TimeMilliseconds",
        "SensorID",
        "Q_Raw_GS_w",
        "Q_Raw_GS_x",
        "Q_Raw_GS_y",
        "Q_Raw_GS_z",
        "Q_Static_CB_w",
        "Q_Static_CB_x",
        "Q_Static_CB_y",
        "Q_Static_CB_z",
        "Q_Final_CB_w",
        "Q_Final_CB_x",
        "Q_Final_CB_y",
        "Q_Final_CB_z",
        "Gyro_S_X_rad_s",
        "Gyro_S_Y_rad_s",
        "Gyro_S_Z_rad_s",
        "Acceleration_S_X_m_s2",
        "Acceleration_S_Y_m_s2",
        "Acceleration_S_Z_m_s2",
        "MagneticField_S_X_au",
        "MagneticField_S_Y_au",
        "MagneticField_S_Z_au",
    ]
    assert OPEN_SIM_JOINT_ANGLE_CSV_PREFIX.split(",") == [
        "Sequence",
        "TimestampMicroseconds",
        "StreamTimeSeconds",
    ]


@pytest.mark.parametrize(
    ("entered_name", "safe_name"),
    (
        ("Patient 01 / chair", "Patient_01_chair"),
        (" Patient 08 / chair ", "Patient_08_chair"),
        ("P01--trial_2", "P01--trial_2"),
        ("///", "Session"),
        ("", "Session"),
    ),
)
def test_session_name_examples_match_cpp(entered_name, safe_name):
    assert safe_session_name(entered_name) == safe_name
