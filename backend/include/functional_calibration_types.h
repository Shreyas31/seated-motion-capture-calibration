#pragma once

#include <Eigen>
#include <string>
#include <vector>

namespace calibration {

/**
 * Identifies the anatomical motion used to estimate a functional joint axis.
 * @enum FunctionalMovementType
 */
enum class FunctionalMovementType {
    FlexionExtension,
    PronationSupination,
    ShoulderAbductionAdduction
};

/**
 * Defines the segment pair and expected anatomical axes for a functional calibration.
 * @struct JointCalibrationDefinition
 */
struct JointCalibrationDefinition {
    std::string displayName;
    std::string proximalSegment;
    std::string distalSegment;
    FunctionalMovementType movementType = FunctionalMovementType::FlexionExtension;
    Eigen::Vector3d proximalAxisB = Eigen::Vector3d::UnitZ();
    Eigen::Vector3d distalAxisB = Eigen::Vector3d::UnitZ();
};

/** Definitions for every functional movement supported by the application. */
inline const std::vector<JointCalibrationDefinition> JOINT_CALIBRATIONS = {
    {"Right knee", "Right_Upperleg", "Right_Lowerleg", FunctionalMovementType::FlexionExtension},
    {"Left knee", "Left_Upperleg", "Left_Lowerleg", FunctionalMovementType::FlexionExtension},
    {"Right elbow flexion/extension", "Right_Upperarm", "Right_Forearm",
     FunctionalMovementType::FlexionExtension},
    {"Right forearm pronation/supination", "Right_Upperarm", "Right_Forearm",
     FunctionalMovementType::PronationSupination},
    {"Right shoulder abduction/adduction", "Sternum", "Right_Upperarm",
     FunctionalMovementType::ShoulderAbductionAdduction, Eigen::Vector3d::UnitX(),
     Eigen::Vector3d::UnitX()},
    {"Left elbow flexion/extension", "Left_Upperarm", "Left_Forearm",
     FunctionalMovementType::FlexionExtension},
    {"Left forearm pronation/supination", "Left_Upperarm", "Left_Forearm",
     FunctionalMovementType::PronationSupination},
    {"Left shoulder abduction/adduction", "Sternum", "Left_Upperarm",
     FunctionalMovementType::ShoulderAbductionAdduction, Eigen::Vector3d::UnitX(),
     Eigen::Vector3d::UnitX()}};

} // namespace calibration
