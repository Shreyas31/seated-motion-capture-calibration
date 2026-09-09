from PySide6.QtWidgets import QGroupBox, QLabel, QPushButton, QSizePolicy, QWidget

from frontend.sensor_mapping import CANONICAL_SEGMENTS
from frontend.setup_page import SetupPageHandlers, build_setup_page
from frontend.workflow_ui import WorkflowContent, build_workflow_shell


def make_setup_page():
    def noop(*_args):
        return None

    return build_setup_page(
        SetupPageHandlers(
            update_controls=noop,
            start_sensor_system=noop,
            run_system_check=noop,
            reload_sensor_mapping=noop,
            configure_sensor_mapping=noop,
            toggle_sensor_table=noop,
            reset_heading_offsets=noop,
            submit_session=noop,
            generate_model=noop,
            patient_inputs_changed=noop,
        ),
        CANONICAL_SEGMENTS,
    )


def make_content(setup_page=None):
    return WorkflowContent(
        setup_page=setup_page or QWidget(),
        calibration_page=QWidget(),
        recording_page=QWidget(),
        diagnostics_page=QWidget(),
        status_label=QLabel(),
        guidance_label=QLabel(),
        compact_session_label=QLabel(),
    )


def test_workflow_shell_contains_four_pages(qtbot):
    shell = build_workflow_shell(make_content(), lambda _index: None)
    qtbot.addWidget(shell.central_widget)

    assert shell.workflow_stack.count() == 4
    assert [button.text() for button in shell.workflow_buttons] == [
        "1  Setup",
        "2  Calibration",
        "3  Recording",
        "Diagnostics",
    ]


def test_navigation_button_reports_selected_page(qtbot):
    selected_pages = []
    shell = build_workflow_shell(make_content(), selected_pages.append)
    qtbot.addWidget(shell.central_widget)

    shell.workflow_buttons[1].click()

    assert selected_pages == [1]


def test_setup_top_row_contains_system_and_mapping_groups(qtbot):
    setup_ui = make_setup_page()
    shell = build_workflow_shell(make_content(setup_ui.page), lambda _index: None)
    qtbot.addWidget(shell.central_widget)

    setup_page = shell.workflow_stack.widget(0)
    sensor_group = setup_page.findChild(QGroupBox, "sensorSystemGroup")
    mapping_group = setup_page.findChild(QGroupBox, "sensorMappingGroup")
    session_details_group = setup_page.findChild(QGroupBox, "sessionDetailsGroup")

    assert sensor_group is not None
    assert mapping_group is not None
    assert session_details_group is not None
    assert sensor_group.title() == "Sensor system"
    assert mapping_group.title() == "Sensor-to-segment mapping"
    assert session_details_group.title() == "Session details"

    setup_layout = setup_page.layout()
    top_layout = setup_layout.itemAt(0).layout()
    assert top_layout is not None
    assert top_layout.itemAt(0).widget() is sensor_group
    assert top_layout.itemAt(1).widget() is mapping_group
    assert setup_layout.itemAt(1).widget() is session_details_group
    assert sensor_group.sizePolicy().verticalPolicy() == QSizePolicy.Policy.Preferred
    assert mapping_group.sizePolicy().verticalPolicy() == QSizePolicy.Policy.Preferred


def test_sensor_system_controls_are_stacked_vertically(qtbot):
    setup_ui = make_setup_page()
    shell = build_workflow_shell(make_content(setup_ui.page), lambda _index: None)
    qtbot.addWidget(shell.central_widget)

    sensor_group = shell.workflow_stack.widget(0).findChild(
        QGroupBox, "sensorSystemGroup"
    )
    assert sensor_group is not None
    assert sensor_group.isAncestorOf(setup_ui.reset_headings_button)

    sensor_layout = sensor_group.layout()
    assert sensor_layout.itemAt(0).widget() is setup_ui.start_backend_button
    assert sensor_layout.itemAt(1).widget() is setup_ui.system_check_button
    assert sensor_layout.itemAt(2).widget() is setup_ui.reset_headings_button
    assert sensor_layout.count() == 3


def test_setup_buttons_keep_fixed_vertical_size_policy():
    setup_ui = make_setup_page()

    for button in (
        setup_ui.start_backend_button,
        setup_ui.system_check_button,
        setup_ui.reset_headings_button,
    ):
        assert button.sizePolicy().verticalPolicy() == QSizePolicy.Policy.Fixed
        assert isinstance(button, QPushButton)
