#pragma once

#include <string_view>

namespace SeatedMoCap::CsvSchema {

/**
 * Column order for long-format raw, static-calibrated, and final-calibrated
 * IMU samples.
 *
 * Gyroscope, acceleration, and magnetic-field vectors are expressed in the
 * sensor frame. Acceleration includes gravity.
 */
inline constexpr std::string_view kMeasurementHeader =
    "PacketID,TimeMilliseconds,SensorID,"
    "Q_Raw_GS_w,Q_Raw_GS_x,Q_Raw_GS_y,Q_Raw_GS_z,"
    "Q_Static_CB_w,Q_Static_CB_x,Q_Static_CB_y,Q_Static_CB_z,"
    "Q_Final_CB_w,Q_Final_CB_x,Q_Final_CB_y,Q_Final_CB_z,"
    "Gyro_S_X_rad_s,Gyro_S_Y_rad_s,Gyro_S_Z_rad_s,"
    "Acceleration_S_X_m_s2,Acceleration_S_Y_m_s2,Acceleration_S_Z_m_s2,"
    "MagneticField_S_X_au,MagneticField_S_Y_au,MagneticField_S_Z_au";

/** Fixed leading columns written before model-dependent OpenSim coordinate columns. */
inline constexpr std::string_view kOpenSimJointAnglePrefix =
    "Sequence,TimestampMicroseconds,StreamTimeSeconds";

} // namespace SeatedMoCap::CsvSchema
