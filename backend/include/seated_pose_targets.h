#pragma once

#include <Eigen>
#include <map>
#include <string>

/**
 * Returns the elbow flexion/extension axis expressed in the neutral
 * palms-together forearm anatomical frame B.
 * @param rightSide True for the right forearm and false for the left forearm.
 * @return Unit elbow flexion-extension axis expressed in forearm frame B.
 */
Eigen::Vector3d buildForearmElbowAxisTarget(bool rightSide);

/**
 * Generates segment-orientation targets for chair sitting with approximately
 * 90-degree knees and the palms facing each other.
 * @return Body-to-session target quaternions q_CB keyed by canonical segment name.
 */
std::map<std::string, Eigen::Quaterniond> buildChairPoseTargets();

/**
 * Generates segment-orientation targets for bed sitting with the legs extended
 * and the palms facing each other.
 * @return Body-to-session target quaternions q_CB keyed by canonical segment name.
 */
std::map<std::string, Eigen::Quaterniond> buildBedPoseTargets();
