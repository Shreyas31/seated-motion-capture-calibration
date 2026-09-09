import json
import os
import re
from dataclasses import asdict, dataclass
from pathlib import Path


@dataclass(frozen=True, slots=True)
class PatientAnthropometry:
    subject_id: str
    patient_height: float
    foot_length: float | None = None
    patient_mass_kg: float | None = None
    schema_version: int = 1
    units: str = "m"

    def to_document(self) -> dict[str, object]:
        return asdict(self)


@dataclass(frozen=True, slots=True)
class PatientModelFiles:
    directory: Path
    anthropometry_file: Path
    model_file: Path


def safe_session_name(session_name: str) -> str:
    safe_name = re.sub(r"[^A-Za-z0-9_-]+", "_", session_name).strip("_")
    return safe_name or "Session"


def safe_subject_id(subject_id: str) -> str:
    safe_id = re.sub(r"[^A-Za-z0-9_.-]+", "_", subject_id).strip("._")

    if not safe_id:
        raise ValueError("The subject ID contains no usable characters.")

    return safe_id


def session_data_directory(
    data_directory: Path,
    session_name: str,
) -> Path:
    """Return the session-specific directory beneath the data root."""
    return Path(data_directory) / safe_session_name(session_name)


def next_joint_angle_csv_path(
    data_directory: Path,
    session_name: str,
) -> Path:
    safe_name = safe_session_name(session_name)
    session_directory = session_data_directory(data_directory, safe_name)
    trial = 1

    while True:
        measurement_path = session_directory / f"{safe_name}_Measurement_{trial}.csv"
        joint_angle_path = session_directory / (
            f"{safe_name}_Measurement_{trial}_JointAngles.csv"
        )

        if not measurement_path.exists() and not joint_angle_path.exists():
            return joint_angle_path

        trial += 1


def patient_model_files(
    data_directory: Path,
    subject_id: str,
) -> PatientModelFiles:
    safe_id = safe_subject_id(subject_id)
    model_directory = session_data_directory(data_directory, safe_id) / "model"
    return PatientModelFiles(
        directory=model_directory,
        anthropometry_file=(model_directory / "patient_anthropometry.json"),
        model_file=model_directory / "Rajagopal_SeatedMoCap.osim",
    )


def write_patient_anthropometry(
    path: Path,
    anthropometry: PatientAnthropometry,
) -> None:
    output_path = Path(path)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    temporary_path = output_path.with_suffix(output_path.suffix + ".tmp")

    try:
        with temporary_path.open("w", encoding="utf-8") as output_file:
            json.dump(anthropometry.to_document(), output_file, indent=2)
            output_file.write("\n")
            output_file.flush()
            os.fsync(output_file.fileno())

        os.replace(temporary_path, output_path)
    except OSError:
        try:
            temporary_path.unlink(missing_ok=True)
        except OSError:
            pass

        raise
