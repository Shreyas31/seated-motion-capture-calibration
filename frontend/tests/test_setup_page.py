from PySide6.QtWidgets import (
    QFormLayout,
    QSizePolicy,
)

from frontend.setup_page import (
    SetupPageHandlers,
    build_setup_page,
)

SEGMENTS = ("Pelvis", "Sternum", "Head")


def handlers(events):
    def record(name):
        return lambda *_args: events.append(name)

    return SetupPageHandlers(
        update_controls=record("update"),
        start_sensor_system=record("start"),
        run_system_check=record("check"),
        reload_sensor_mapping=record("reload"),
        configure_sensor_mapping=record("configure"),
        toggle_sensor_table=lambda visible: events.append(("toggle", visible)),
        reset_heading_offsets=record("reset-heading"),
        submit_session=record("submit"),
        generate_model=record("generate"),
        patient_inputs_changed=record("patient-input"),
    )


def test_patient_sensor_defaults(qtbot):
    ui = build_setup_page(handlers([]), SEGMENTS)
    qtbot.addWidget(ui.patient_group)

    assert ui.pose_box.currentData() == "chair"
    assert ui.pose_box.count() == 2
    assert [ui.pose_box.itemData(index) for index in range(ui.pose_box.count())] == [
        "chair",
        "bed",
    ]
    assert ui.height_input.value() == 1.70
    assert ui.foot_length_input.isEnabled()
    assert ui.sensor_mapping_table.rowCount() == len(SEGMENTS)
    assert not ui.sensor_mapping_table.isVisible()
    assert ui.patient_group.isAncestorOf(ui.session_name_input)
    assert ui.patient_group.title() == ("Session details")

    assert ui.patient_group.isAncestorOf(ui.model_summary_label)
    assert ui.patient_group.isAncestorOf(ui.generate_model_button)
    assert ui.patient_group.isAncestorOf(ui.submit_session_button)
    assert ui.pose_box.sizePolicy().horizontalPolicy() == QSizePolicy.Policy.Expanding
    assert (
        ui.height_input.sizePolicy().horizontalPolicy() == QSizePolicy.Policy.Expanding
    )
    assert (
        ui.foot_length_input.sizePolicy().horizontalPolicy()
        == QSizePolicy.Policy.Expanding
    )
    assert ui.patient_group.isAncestorOf(ui.pose_box)
    assert not hasattr(
        ui,
        "use_foot_length",
    )


def test_session_actions_share_bottom_row(
    qtbot,
):
    ui = build_setup_page(
        handlers([]),
        SEGMENTS,
    )
    qtbot.addWidget(ui.patient_group)

    form_layout = ui.patient_group.layout()

    action_row_index = form_layout.rowCount() - 1

    action_item = form_layout.itemAt(
        action_row_index,
        QFormLayout.ItemRole.SpanningRole,
    )

    assert action_item is not None

    action_layout = action_item.layout()

    assert action_layout is not None
    assert action_layout.itemAt(0).widget() is ui.model_summary_label
    assert action_layout.itemAt(2).widget() is ui.generate_model_button
    assert action_layout.itemAt(3).widget() is ui.submit_session_button


def test_patient_sensor_buttons_use_supplied_handlers(qtbot):
    events = []
    ui = build_setup_page(handlers(events), SEGMENTS)
    qtbot.addWidget(ui.patient_group)

    ui.start_backend_button.click()
    ui.system_check_button.click()
    ui.reset_headings_button.click()

    ui.generate_model_button.click()
    ui.submit_session_button.click()

    assert events == [
        "start",
        "check",
        "reset-heading",
        "generate",
        "submit",
    ]


def test_manual_sensor_continue_button_is_removed():
    ui = build_setup_page(
        handlers([]),
        SEGMENTS,
    )

    assert not hasattr(
        ui,
        "sensors_ready_button",
    )
    assert not hasattr(
        ui,
        "acquisition_group",
    )
