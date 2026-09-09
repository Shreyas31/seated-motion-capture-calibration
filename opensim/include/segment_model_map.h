#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace SeatedMoCap {

/**
 * Identifies whether a segment uses an original or project-added model body.
 * @enum BodyTreatment.
 */
enum class BodyTreatment { ExistingRajagopalBody, AddedByAugmentation };

/**
 * Maps one UDP segment index to its OpenSim body and virtual IMU frame.
 * @struct SegmentModelMapping.
 */
struct SegmentModelMapping {
    std::size_t index;
    std::string_view backendSegment;
    std::string_view opensimBody;
    std::string_view imuFrame;
    BodyTreatment treatment;
};

/** Fixed protocol-order mapping shared by model generation, IK, and UDP processing. */
inline constexpr auto kSegmentModelMappings = std::to_array<SegmentModelMapping>(
    {{0, "Pelvis", "pelvis", "pelvis_imu", BodyTreatment::ExistingRajagopalBody},

     {1, "Sternum", "torso", "torso_imu", BodyTreatment::ExistingRajagopalBody},

     {2, "Head", "head_mocap", "head_mocap_imu", BodyTreatment::AddedByAugmentation},

     {3, "Right_Shoulder", "shoulder_girdle_r_mocap", "shoulder_girdle_r_mocap_imu",
      BodyTreatment::AddedByAugmentation},

     {4, "Right_Upperarm", "humerus_r", "humerus_r_imu", BodyTreatment::ExistingRajagopalBody},

     {5, "Right_Forearm", "radius_r", "radius_r_imu", BodyTreatment::ExistingRajagopalBody},

     {6, "Right_Hand", "hand_r", "hand_r_imu", BodyTreatment::ExistingRajagopalBody},

     {7, "Left_Shoulder", "shoulder_girdle_l_mocap", "shoulder_girdle_l_mocap_imu",
      BodyTreatment::AddedByAugmentation},

     {8, "Left_Upperarm", "humerus_l", "humerus_l_imu", BodyTreatment::ExistingRajagopalBody},

     {9, "Left_Forearm", "radius_l", "radius_l_imu", BodyTreatment::ExistingRajagopalBody},

     {10, "Left_Hand", "hand_l", "hand_l_imu", BodyTreatment::ExistingRajagopalBody},

     {11, "Right_Upperleg", "femur_r", "femur_r_imu", BodyTreatment::ExistingRajagopalBody},

     {12, "Right_Lowerleg", "tibia_r", "tibia_r_imu", BodyTreatment::ExistingRajagopalBody},

     {13, "Right_Foot", "calcn_r", "calcn_r_imu", BodyTreatment::ExistingRajagopalBody},

     {14, "Left_Upperleg", "femur_l", "femur_l_imu", BodyTreatment::ExistingRajagopalBody},

     {15, "Left_Lowerleg", "tibia_l", "tibia_l_imu", BodyTreatment::ExistingRajagopalBody},

     {16, "Left_Foot", "calcn_l", "calcn_l_imu", BodyTreatment::ExistingRajagopalBody}});

} // namespace SeatedMoCap
