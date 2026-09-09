from frontend.recording_page import RecordingPageHandlers, build_recording_page


def handlers(events):
    def record(name):
        return lambda *_args: events.append(name)

    return RecordingPageHandlers(
        start_viewer=record("start-viewer"),
        stop_viewer=record("stop-viewer"),
        toggle_recording=record("toggle-recording"),
        exit_sensor_system=record("exit"),
    )


def test_recording_page_defaults(qtbot):
    ui = build_recording_page(handlers([]))
    qtbot.addWidget(ui.page)

    assert ui.recording_elapsed_label.text() == "00:00"
    assert not ui.stationary_feet_checkbox.isChecked()
    assert ui.recording_primary_button.minimumHeight() == 52


def test_recording_buttons_use_handlers(qtbot):
    events = []
    ui = build_recording_page(handlers(events))
    qtbot.addWidget(ui.page)

    ui.start_viewer_button.click()
    ui.stop_viewer_button.click()
    ui.recording_primary_button.click()
    ui.exit_backend_button.click()

    assert events == [
        "start-viewer",
        "stop-viewer",
        "toggle-recording",
        "exit",
    ]
