import logging
import os
import subprocess
import sys
from logging.handlers import RotatingFileHandler
from pathlib import Path

from frontend.application_bootstrap import configure_application_logging


def test_importing_app_does_not_create_output_directory(tmp_path):
    output_directory = tmp_path / "must-not-be-created"
    project_root = Path(__file__).resolve().parents[2]
    environment = os.environ.copy()
    environment["SEATED_MOCAP_OUTPUT_DIR"] = str(output_directory)
    environment["QT_QPA_PLATFORM"] = "offscreen"

    result = subprocess.run(
        [sys.executable, "-c", "import frontend.app"],
        cwd=project_root,
        env=environment,
        capture_output=True,
        text=True,
        check=False,
    )

    assert result.returncode == 0, result.stderr
    assert not output_directory.exists()


def test_main_runs_runtime_setup_before_showing_window(monkeypatch):
    import frontend.app as app_module

    calls = []

    class FakeApplication:
        def __init__(self, arguments):
            calls.append(("application", arguments))

        def exec(self):
            calls.append(("exec",))
            return 23

    class FakeWindow:
        def show(self):
            calls.append(("show",))

    monkeypatch.setattr(app_module.sys, "argv", ["frontend"])
    monkeypatch.setattr(
        app_module,
        "add_opensim_runtime_to_path",
        lambda: calls.append(("runtime",)),
    )
    monkeypatch.setattr(
        app_module,
        "configure_application_logging",
        lambda _directory: calls.append(("logging",)),
    )
    monkeypatch.setattr(app_module, "QApplication", FakeApplication)
    monkeypatch.setattr(
        app_module,
        "load_application_stylesheet",
        lambda _application, **_kwargs: calls.append(("stylesheet",)),
    )
    monkeypatch.setattr(app_module, "MainWindow", FakeWindow)

    assert app_module.main() == 23
    assert calls == [
        ("runtime",),
        ("logging",),
        ("application", ["frontend"]),
        ("stylesheet",),
        ("show",),
        ("exec",),
    ]


def test_launchers_use_package_module_entry_point():
    repository = Path(__file__).resolve().parents[2]
    launchers = (
        repository / "launch_frontend.cmd",
        repository / "deployment" / "launch_frontend.cmd",
    )

    for launcher in launchers:
        contents = launcher.read_text(encoding="utf-8")
        assert "-m frontend.app" in contents
        assert '"frontend\\app.py"' not in contents


def test_logging_configuration_creates_runtime_log(tmp_path):
    logger_name = "irp_seated_mocap.test.create"
    logger = configure_application_logging(
        tmp_path,
        logger_name=logger_name,
    )

    try:
        logger.info("test message")
        for handler in logger.handlers:
            handler.flush()

        log_file = tmp_path / "logs" / "frontend.log"
        assert log_file.is_file()
        assert "test message" in log_file.read_text(encoding="utf-8")
    finally:
        close_handlers(logger)


def test_logging_configuration_is_idempotent(tmp_path):
    logger_name = "irp_seated_mocap.test.idempotent"
    logger = configure_application_logging(
        tmp_path,
        logger_name=logger_name,
    )

    try:
        configure_application_logging(tmp_path, logger_name=logger_name)
        rotating_handlers = [
            handler
            for handler in logger.handlers
            if isinstance(handler, RotatingFileHandler)
        ]
        assert len(rotating_handlers) == 1
    finally:
        close_handlers(logger)


def close_handlers(logger):
    for handler in tuple(logger.handlers):
        logger.removeHandler(handler)
        handler.close()

    logging.Logger.manager.loggerDict.pop(logger.name, None)
