import pytest

from frontend.protocol import FrontendProtocolEvent, parse_frontend_protocol_line
from frontend.sensor_mapping import EXPECTED_SENSOR_COUNT


@pytest.mark.parametrize(
    ("line", "expected"),
    (
        (
            "FRONTEND|STATE|SENSORS",
            FrontendProtocolEvent("STATE", "SENSORS"),
        ),
        (
            f"FRONTEND|RESULT|SENSORS_CONNECTED|{EXPECTED_SENSOR_COUNT}\r\n",
            FrontendProtocolEvent(
                "RESULT", "SENSORS_CONNECTED", str(EXPECTED_SENSOR_COUNT)
            ),
        ),
        (
            " FRONTEND|ERROR|RECORDING|could not open|trial.csv ",
            FrontendProtocolEvent(
                "ERROR",
                "RECORDING",
                "could not open|trial.csv",
            ),
        ),
        (
            "FRONTEND|GUIDANCE|COUNTDOWN|STATIC:3",
            FrontendProtocolEvent("GUIDANCE", "COUNTDOWN", "STATIC:3"),
        ),
    ),
)
def test_parse_frontend_protocol_line(line, expected):
    assert parse_frontend_protocol_line(line) == expected


@pytest.mark.parametrize(
    "line",
    (
        "ordinary console output",
        "FRONTEND",
        "FRONTEND|STATE",
        "FRONTEND|UNKNOWN|VALUE",
        "FRONTEND|RESULT|",
    ),
)
def test_invalid_frontend_protocol_line_is_ignored(line):
    assert parse_frontend_protocol_line(line) is None
