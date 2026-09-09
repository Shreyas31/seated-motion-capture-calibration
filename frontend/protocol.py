from dataclasses import dataclass

PROTOCOL_PREFIX = "FRONTEND"


class Category:
    """Top-level categories emitted by the sensor backend."""

    STATE = "STATE"
    RESULT = "RESULT"
    ERROR = "ERROR"
    GUIDANCE = "GUIDANCE"


class StateName:
    """Stable backend workflow state names."""

    SENSORS = "SENSORS"
    HEADING = "HEADING"
    SESSION = "SESSION"
    MENU = "MENU"
    JOINT = "JOINT"
    RECORDING = "RECORDING"
    EXITING = "EXITING"


class ResultName:
    """Stable names for successful backend results."""

    SENSOR_STATUS = "SENSOR_STATUS"
    UNMAPPED_SENSOR = "UNMAPPED_SENSOR"
    SENSORS_CONNECTED = "SENSORS_CONNECTED"
    SESSION_CONFIGURED = "SESSION_CONFIGURED"
    STATIC_CALIBRATION = "STATIC_CALIBRATION"
    FUNCTIONAL_CALIBRATION = "FUNCTIONAL_CALIBRATION"
    RECORDING_STARTED = "RECORDING_STARTED"
    RECORDING_STOPPED = "RECORDING_STOPPED"


class ErrorName:
    """Stable names for backend errors handled by the frontend."""

    SENSORS_INCOMPLETE = "SENSORS_INCOMPLETE"
    STATIC_CALIBRATION = "STATIC_CALIBRATION"
    FUNCTIONAL_CALIBRATION = "FUNCTIONAL_CALIBRATION"
    RECORDING = "RECORDING"


class GuidanceName:
    """Stable clinician-guidance phase names."""

    STATIC_PREPARATION = "STATIC_PREPARATION"
    STATIC_CAPTURE = "STATIC_CAPTURE"
    STATIC_PROCESSING = "STATIC_PROCESSING"
    FUNCTIONAL_GRAVITY_PREPARATION = "FUNCTIONAL_GRAVITY_PREPARATION"
    FUNCTIONAL_GRAVITY_CAPTURE = "FUNCTIONAL_GRAVITY_CAPTURE"
    FUNCTIONAL_GRAVITY_PROCESSING = "FUNCTIONAL_GRAVITY_PROCESSING"
    FUNCTIONAL_MOVEMENT_PREPARATION = "FUNCTIONAL_MOVEMENT_PREPARATION"
    FUNCTIONAL_MOVEMENT_CAPTURE = "FUNCTIONAL_MOVEMENT_CAPTURE"
    FUNCTIONAL_RETURN_POSE_PREPARATION = "FUNCTIONAL_RETURN_POSE_PREPARATION"
    FUNCTIONAL_RETURN_POSE_CAPTURE = "FUNCTIONAL_RETURN_POSE_CAPTURE"
    FUNCTIONAL_PROCESSING = "FUNCTIONAL_PROCESSING"
    COUNTDOWN = "COUNTDOWN"


class ViewerEventName:
    """Stable names emitted by the separate OpenSim viewer protocol."""

    READY = "READY"
    STREAM_ENDED = "STREAM_ENDED"
    CLOSED = "CLOSED"


class SensorStatus:
    """Values carried in SENSOR_STATUS result details."""

    CONNECTED = "CONNECTED"
    DISCONNECTED = "DISCONNECTED"


PROTOCOL_CATEGORIES = (
    Category.STATE,
    Category.RESULT,
    Category.ERROR,
    Category.GUIDANCE,
)
STATE_NAMES = (
    StateName.SENSORS,
    StateName.HEADING,
    StateName.SESSION,
    StateName.MENU,
    StateName.JOINT,
    StateName.RECORDING,
    StateName.EXITING,
)
RESULT_NAMES = (
    ResultName.SENSOR_STATUS,
    ResultName.UNMAPPED_SENSOR,
    ResultName.SENSORS_CONNECTED,
    ResultName.SESSION_CONFIGURED,
    ResultName.STATIC_CALIBRATION,
    ResultName.FUNCTIONAL_CALIBRATION,
    ResultName.RECORDING_STARTED,
    ResultName.RECORDING_STOPPED,
)
ERROR_NAMES = (
    ErrorName.SENSORS_INCOMPLETE,
    ErrorName.STATIC_CALIBRATION,
    ErrorName.FUNCTIONAL_CALIBRATION,
    ErrorName.RECORDING,
)
GUIDANCE_NAMES = (
    GuidanceName.STATIC_PREPARATION,
    GuidanceName.STATIC_CAPTURE,
    GuidanceName.STATIC_PROCESSING,
    GuidanceName.FUNCTIONAL_GRAVITY_PREPARATION,
    GuidanceName.FUNCTIONAL_GRAVITY_CAPTURE,
    GuidanceName.FUNCTIONAL_GRAVITY_PROCESSING,
    GuidanceName.FUNCTIONAL_MOVEMENT_PREPARATION,
    GuidanceName.FUNCTIONAL_MOVEMENT_CAPTURE,
    GuidanceName.FUNCTIONAL_RETURN_POSE_PREPARATION,
    GuidanceName.FUNCTIONAL_RETURN_POSE_CAPTURE,
    GuidanceName.FUNCTIONAL_PROCESSING,
    GuidanceName.COUNTDOWN,
)
VIEWER_EVENT_NAMES = (
    ViewerEventName.READY,
    ViewerEventName.STREAM_ENDED,
    ViewerEventName.CLOSED,
)

# Numeric choices are duplicated by calibration::JOINT_CALIBRATIONS in C++.
FUNCTIONAL_CALIBRATIONS = (
    ("Right knee", 1),
    ("Left knee", 2),
    ("Right elbow flexion/extension", 3),
    ("Right forearm pronation/supination", 4),
    ("Right shoulder abduction/adduction", 5),
    ("Left elbow flexion/extension", 6),
    ("Left forearm pronation/supination", 7),
    ("Left shoulder abduction/adduction", 8),
)

# These mirror shared/include/realtime_orientation_packet.h for contract tests.
UDP_PROTOCOL_VERSION = 1
UDP_PACKET_SIZE_BYTES = 300
UDP_SEGMENT_ORDER = (
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

# These mirror shared/include/csv_schemas.h for contract tests.
MEASUREMENT_CSV_HEADER = (
    "PacketID,TimeMilliseconds,SensorID,"
    "Q_Raw_GS_w,Q_Raw_GS_x,Q_Raw_GS_y,Q_Raw_GS_z,"
    "Q_Static_CB_w,Q_Static_CB_x,Q_Static_CB_y,Q_Static_CB_z,"
    "Q_Final_CB_w,Q_Final_CB_x,Q_Final_CB_y,Q_Final_CB_z,"
    "Gyro_S_X_rad_s,Gyro_S_Y_rad_s,Gyro_S_Z_rad_s,"
    "Acceleration_S_X_m_s2,Acceleration_S_Y_m_s2,Acceleration_S_Z_m_s2,"
    "MagneticField_S_X_au,MagneticField_S_Y_au,MagneticField_S_Z_au"
)
OPEN_SIM_JOINT_ANGLE_CSV_PREFIX = "Sequence,TimestampMicroseconds,StreamTimeSeconds"


@dataclass(frozen=True, slots=True)
class FrontendProtocolEvent:
    category: str
    name: str
    detail: str = ""


SUPPORTED_CATEGORIES = frozenset(PROTOCOL_CATEGORIES)


def parse_frontend_protocol_line(line: str) -> FrontendProtocolEvent | None:
    """Parse one complete line emitted by the C++ sensor system."""
    stripped_line = line.strip()
    if not stripped_line.startswith(f"{PROTOCOL_PREFIX}|"):
        return None

    parts = stripped_line.split("|", 3)
    if len(parts) < 3:
        return None

    category = parts[1]
    name = parts[2]
    if category not in SUPPORTED_CATEGORIES or not name:
        return None

    detail = parts[3] if len(parts) == 4 else ""
    return FrontendProtocolEvent(category, name, detail)
