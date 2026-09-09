from frontend.calibration_page import (
    CalibrationPageHandlers,
    build_calibration_page,
)
from frontend.protocol import FUNCTIONAL_CALIBRATIONS


def handlers(events):
    def record(name):
        return lambda *_args: events.append(name)

    return CalibrationPageHandlers(
        start_static_calibration=record("static"),
        start_functional_calibration=record("start-functional"),
    )


def test_functional_calibration_backend_choices():
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


def test_calibration_page_defaults(qtbot):
    ui = build_calibration_page(handlers([]))
    qtbot.addWidget(ui.page)

    assert ui.joint_box.count() == len(FUNCTIONAL_CALIBRATIONS)
    assert ui.joint_box.currentData() == 1
    assert set(ui.functional_status_labels) == {
        name for name, _choice in FUNCTIONAL_CALIBRATIONS
    }


def test_calibration_buttons_use_handlers(qtbot):
    events = []
    ui = build_calibration_page(handlers(events))
    qtbot.addWidget(ui.page)

    ui.static_calibration_button.click()
    ui.start_joint_button.click()

    assert events == ["static", "start-functional"]
