import logging
from logging.handlers import RotatingFileHandler
from pathlib import Path

from PySide6.QtCore import Qt
from PySide6.QtGui import QPalette
from PySide6.QtWidgets import QApplication

LOGGER_NAME = "irp_seated_mocap"
LOG_FORMAT = "%(asctime)s | %(levelname)s | %(message)s"


def get_application_logger() -> logging.Logger:
    """Return the application logger without configuring filesystem output."""

    return logging.getLogger(LOGGER_NAME)


def configure_application_logging(
    data_directory: Path,
    *,
    logger_name: str = LOGGER_NAME,
) -> logging.Logger:
    """Create the runtime log directory and attach one rotating file handler."""

    log_directory = Path(data_directory) / "logs"
    log_directory.mkdir(parents=True, exist_ok=True)
    log_file = (log_directory / "frontend.log").resolve()
    logger = logging.getLogger(logger_name)
    logger.setLevel(logging.INFO)

    for handler in logger.handlers:
        if (
            isinstance(handler, RotatingFileHandler)
            and Path(handler.baseFilename).resolve() == log_file
        ):
            return logger

    handler = RotatingFileHandler(
        log_file,
        maxBytes=5_000_000,
        backupCount=5,
        encoding="utf-8",
    )
    handler.setFormatter(logging.Formatter(LOG_FORMAT))
    logger.addHandler(handler)
    return logger


def load_application_stylesheet(
    application: QApplication,
    *,
    stylesheet_directory: Path | None = None,
    logger: logging.Logger | None = None,
) -> None:
    colour_scheme = application.styleHints().colorScheme()

    if colour_scheme == Qt.ColorScheme.Dark:
        dark_theme = True
    elif colour_scheme == Qt.ColorScheme.Light:
        dark_theme = False
    else:
        window_colour = application.palette().color(QPalette.ColorRole.Window)
        dark_theme = window_colour.lightness() < 128

    stylesheet_name = "style_dark.qss" if dark_theme else "style.qss"
    source_directory = (
        Path(__file__).parent
        if stylesheet_directory is None
        else Path(stylesheet_directory)
    )
    stylesheet_path = source_directory / stylesheet_name
    application.setProperty("darkTheme", dark_theme)

    try:
        application.setStyleSheet(stylesheet_path.read_text(encoding="utf-8"))
    except OSError as error:
        active_logger = logger or get_application_logger()
        active_logger.warning(
            "Could not load frontend stylesheet %s: %s",
            stylesheet_path,
            error,
        )
