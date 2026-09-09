#include "seated_calibration.h"

#include "angular_uncertainty.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

using namespace Eigen;

Quaterniond SeatedCalibration::computeStaticOffset(const Quaterniond& q_CS,
                                                   const Quaterniond& q_CB) {
    // Xsens defines q_CS as the rotation from sensor coordinates S to the
    // calibrated frame C. The Xsens MVN calibration equation is
    // q_CB = q_CS * conjugate(q_BS), hence q_BS = conjugate(q_CB) * q_CS.
    return (q_CB.normalized().conjugate() * q_CS.normalized()).normalized();
}

FunctionalAxisEstimate
SeatedCalibration::computeFunctionalAxis(const std::vector<Vector3d>& angular_velocities,
                                         const Vector3d& gyro_bias, double minimum_speed) {

    FunctionalAxisEstimate result;

    constexpr std::size_t MINIMUM_SAMPLE_COUNT = 20;

    // A hinge is an unoriented line, so accept samples near either axis direction.
    constexpr double MAXIMUM_INLIER_ANGLE_DEGREES = 20.0;

    // Reject captures dominated by motion outside the provisional hinge direction.
    constexpr double MINIMUM_INLIER_FRACTION = 0.60;

    if (angular_velocities.size() < MINIMUM_SAMPLE_COUNT) {
        result.message = "Too few angular-velocity samples.";
        return result;
    }

    // Do not subtract the mean: hinge velocity lies on a line through zero and
    // the two movement directions may have unequal sample counts.
    std::vector<Vector3d> candidate_samples;
    candidate_samples.reserve(angular_velocities.size());

    for (const Vector3d& raw_sample : angular_velocities) {
        const Vector3d sample = raw_sample - gyro_bias;
        const double speed = sample.norm();

        if (!sample.allFinite() || speed < minimum_speed) {
            continue;
        }

        candidate_samples.push_back(sample);
    }

    result.candidateSamples = candidate_samples.size();

    if (candidate_samples.size() < MINIMUM_SAMPLE_COUNT) {
        result.message = "Fewer than 20 samples remained after the speed threshold.";
        return result;
    }

    // Normalize the first-pass samples so one fast accidental motion cannot
    // dominate the provisional PCA through squared angular speed.
    Matrix3d provisional_second_moment = Matrix3d::Zero();

    for (const Vector3d& sample : candidate_samples) {
        const Vector3d direction = sample.normalized();
        provisional_second_moment.noalias() += direction * direction.transpose();
    }

    provisional_second_moment /= static_cast<double>(candidate_samples.size());

    SelfAdjointEigenSolver<Matrix3d> provisional_solver(provisional_second_moment);

    if (provisional_solver.info() != Success) {
        result.message = "Provisional PCA decomposition failed.";
        return result;
    }

    const Vector3d provisional_axis = provisional_solver.eigenvectors().col(2).normalized();

    // Flexion and extension rotate in opposite directions about the same axis.
    const double cosine_limit = std::cos(MAXIMUM_INLIER_ANGLE_DEGREES * std::acos(-1.0) / 180.0);

    std::vector<Vector3d> inlier_samples;
    inlier_samples.reserve(candidate_samples.size());

    for (const Vector3d& sample : candidate_samples) {
        const double line_alignment = std::abs(sample.normalized().dot(provisional_axis));

        if (line_alignment >= cosine_limit) {
            inlier_samples.push_back(sample);
        }
    }

    result.usedSamples = inlier_samples.size();
    result.rejectedOutliers = result.candidateSamples - result.usedSamples;

    result.inlierFraction =
        static_cast<double>(result.usedSamples) / static_cast<double>(result.candidateSamples);

    if (result.usedSamples < MINIMUM_SAMPLE_COUNT) {
        result.message = "Fewer than 20 samples remained after outlier rejection.";
        return result;
    }

    if (result.inlierFraction < MINIMUM_INLIER_FRACTION) {
        result.message = "Too many angular-velocity samples were directional outliers.";
        return result;
    }

    // Restore magnitudes for the final PCA so well-excited inliers carry more weight.
    Matrix3d refined_second_moment = Matrix3d::Zero();
    double squared_speed_sum = 0.0;

    for (const Vector3d& sample : inlier_samples) {
        refined_second_moment.noalias() += sample * sample.transpose();

        squared_speed_sum += sample.squaredNorm();
    }

    refined_second_moment /= static_cast<double>(result.usedSamples);

    SelfAdjointEigenSolver<Matrix3d> refined_solver(refined_second_moment);

    if (refined_solver.info() != Success) {
        result.message = "Refined PCA decomposition failed.";
        return result;
    }

    const Vector3d eigenvalues = refined_solver.eigenvalues();

    const double dominant = eigenvalues[2];
    const double secondary = eigenvalues[1];

    if (!(dominant > std::numeric_limits<double>::epsilon())) {
        result.message = "Angular-velocity excitation has zero variance.";
        return result;
    }

    result.axis = refined_solver.eigenvectors().col(2).normalized();

    result.confidence = std::clamp((dominant - secondary) / dominant, 0.0, 1.0);

    result.rmsAngularSpeed = std::sqrt(squared_speed_sum / static_cast<double>(result.usedSamples));

    if (result.confidence < 0.55) {
        result.message = "Motion was not sufficiently one-dimensional after outlier rejection.";
        return result;
    }

    result.valid = true;
    result.message = "Robust functional axis accepted.";
    return result;
}

FunctionalAxisEstimate SeatedCalibration::computeFunctionalAxisWithUncertainty(
    const std::vector<Vector3d>& angular_velocities, const Vector3d& gyro_bias,
    double minimum_speed, std::size_t requested_block_count, std::size_t minimum_valid_blocks) {

    FunctionalAxisEstimate result =
        computeFunctionalAxis(angular_velocities, gyro_bias, minimum_speed);

    if (!result.valid) {
        return result;
    }

    constexpr std::size_t MINIMUM_SAMPLES_PER_BLOCK = 20;

    if (requested_block_count == 0 || minimum_valid_blocks == 0) {
        result.valid = false;
        result.message = "Functional-axis uncertainty configuration is invalid.";
        return result;
    }

    // Each uncertainty block must independently satisfy the 20-sample minimum.
    const std::size_t maximum_block_count = angular_velocities.size() / MINIMUM_SAMPLES_PER_BLOCK;

    const std::size_t block_count = std::min(requested_block_count, maximum_block_count);

    if (block_count < minimum_valid_blocks) {
        result.valid = false;
        result.message = "Too few samples to estimate functional-axis uncertainty.";
        return result;
    }

    std::vector<double> angular_deviations;
    angular_deviations.reserve(block_count);

    for (std::size_t block = 0; block < block_count; ++block) {
        const std::size_t begin_index = block * angular_velocities.size() / block_count;

        const std::size_t end_index = (block + 1) * angular_velocities.size() / block_count;

        std::vector<Vector3d> block_samples(
            angular_velocities.begin() + static_cast<std::ptrdiff_t>(begin_index),
            angular_velocities.begin() + static_cast<std::ptrdiff_t>(end_index));

        const FunctionalAxisEstimate block_estimate =
            computeFunctionalAxis(block_samples, gyro_bias, minimum_speed);

        if (!block_estimate.valid) {
            continue;
        }

        // Resolve the sign ambiguity of the unoriented PCA axis.
        const double cosine = std::clamp(std::abs(block_estimate.axis.dot(result.axis)), 0.0, 1.0);

        angular_deviations.push_back(std::acos(cosine));
    }

    if (angular_deviations.size() < minimum_valid_blocks) {
        result.valid = false;
        result.message = "Too few valid movement blocks to estimate functional-axis uncertainty.";
        return result;
    }

    const auto uncertainty =
        calibration_statistics::robustMadSigma(angular_deviations, minimum_valid_blocks);

    if (!uncertainty || !std::isfinite(*uncertainty)) {
        result.valid = false;
        result.message = "Functional-axis uncertainty estimation failed.";
        return result;
    }

    result.angularUncertaintyRadians = *uncertainty;
    result.uncertaintyBlocks = angular_deviations.size();
    result.message = "Robust functional axis and block uncertainty accepted.";

    return result;
}

std::optional<Quaterniond>
SeatedCalibration::averageQuaternions(const std::vector<Quaterniond>& quaternions) {

    if (quaternions.empty()) {
        return std::nullopt;
    }

    Matrix4d accumulator = Matrix4d::Zero();
    std::size_t valid_count = 0;
    for (Quaterniond quaternion : quaternions) {
        if (!quaternion.coeffs().allFinite() ||
            quaternion.squaredNorm() < std::numeric_limits<double>::epsilon()) {
            continue;
        }

        quaternion.normalize();
        const Vector4d coefficients = quaternion.coeffs(); // Eigen order: x,y,z,w
        accumulator.noalias() += coefficients * coefficients.transpose();
        ++valid_count;
    }

    if (valid_count == 0) {
        return std::nullopt;
    }

    SelfAdjointEigenSolver<Matrix4d> solver(accumulator);
    if (solver.info() != Success) {
        return std::nullopt;
    }

    const Vector4d mean_coefficients = solver.eigenvectors().col(3);
    Quaterniond mean(mean_coefficients[3], mean_coefficients[0], mean_coefficients[1],
                     mean_coefficients[2]);
    return mean.normalized();
}

std::optional<Quaterniond>
SeatedCalibration::computeGlobalToSessionYaw(const std::vector<Quaterniond>& torso_orientations_GS,
                                             const Vector3d& sensor_forward_axis_S_input) {

    if (torso_orientations_GS.empty() || !sensor_forward_axis_S_input.allFinite() ||
        sensor_forward_axis_S_input.norm() < 1e-9) {
        return std::nullopt;
    }

    const Vector3d sensor_forward_axis_S = sensor_forward_axis_S_input.normalized();
    Vector3d mean_horizontal_forward_G = Vector3d::Zero();
    std::size_t valid_count = 0;
    for (Quaterniond q_GS : torso_orientations_GS) {
        if (!q_GS.coeffs().allFinite() || q_GS.squaredNorm() < 1e-9) {
            continue;
        }
        q_GS.normalize();
        Vector3d forward_G = q_GS * sensor_forward_axis_S;

        // Only yaw defines the session frame; discard torso flexion and recline.
        forward_G.z() = 0.0;
        if (forward_G.norm() < 0.2) {
            continue;
        }
        mean_horizontal_forward_G += forward_G.normalized();
        ++valid_count;
    }

    if (valid_count == 0 || mean_horizontal_forward_G.norm() < 0.2) {
        return std::nullopt;
    }

    mean_horizontal_forward_G.normalize();
    const double earth_heading =
        std::atan2(mean_horizontal_forward_G.y(), mean_horizontal_forward_G.x());

    // Rotate observed patient-forward onto session +X for every sensor.
    return Quaterniond(AngleAxisd(-earth_heading, Vector3d::UnitZ())).normalized();
}

GravityDirectionEstimate
SeatedCalibration::computeGravityDirection(const std::vector<Vector3d>& accelerations,
                                           const std::size_t samplesPerBlock,
                                           const std::size_t minimumBlocks) {

    GravityDirectionEstimate result;

    constexpr double MINIMUM_ACCELERATION_MAGNITUDE = 7.0;

    constexpr double MAXIMUM_ACCELERATION_MAGNITUDE = 12.5;

    constexpr double MAXIMUM_DIRECTION_DEVIATION_DEGREES = 12.0;

    if (samplesPerBlock == 0 || minimumBlocks < 2) {

        result.message = "Invalid gravity block configuration.";

        return result;
    }

    if (accelerations.size() < samplesPerBlock * minimumBlocks) {

        result.message = "Too few acceleration samples for gravity blocks.";

        return result;
    }

    // Seed the direction from finite samples having approximately gravity magnitude.
    Vector3d provisionalSum = Vector3d::Zero();

    std::size_t provisionalCount = 0;

    for (const Vector3d& acceleration : accelerations) {

        if (!acceleration.allFinite()) {
            continue;
        }

        const double magnitude = acceleration.norm();

        if (magnitude <= MINIMUM_ACCELERATION_MAGNITUDE ||
            magnitude >= MAXIMUM_ACCELERATION_MAGNITUDE) {

            continue;
        }

        provisionalSum += acceleration / magnitude;

        ++provisionalCount;
    }

    if (provisionalCount < 10 || provisionalSum.norm() < 1e-9) {

        result.message = "Could not determine provisional gravity direction.";

        return result;
    }

    const Vector3d provisionalDirection = provisionalSum.normalized();

    const double cosineLimit =
        std::cos(MAXIMUM_DIRECTION_DEVIATION_DEGREES * std::acos(-1.0) / 180.0);

    const std::size_t minimumSamplesPerBlock = (samplesPerBlock + 1) / 2;

    std::vector<Vector3d> blockDirections;

    // Preserve temporal blocks even when individual samples are rejected.
    for (std::size_t blockBegin = 0; blockBegin + samplesPerBlock <= accelerations.size();
         blockBegin += samplesPerBlock) {

        Vector3d blockSum = Vector3d::Zero();

        std::size_t acceptedSamples = 0;

        const std::size_t blockEnd = blockBegin + samplesPerBlock;

        for (std::size_t index = blockBegin; index < blockEnd; ++index) {

            const Vector3d& acceleration = accelerations[index];

            if (!acceleration.allFinite()) {
                continue;
            }

            const double magnitude = acceleration.norm();

            if (magnitude <= MINIMUM_ACCELERATION_MAGNITUDE ||
                magnitude >= MAXIMUM_ACCELERATION_MAGNITUDE) {

                continue;
            }

            const Vector3d direction = acceleration / magnitude;

            if (direction.dot(provisionalDirection) < cosineLimit) {

                continue;
            }

            blockSum += direction;
            ++acceptedSamples;
        }

        if (acceptedSamples < minimumSamplesPerBlock || blockSum.norm() < 1e-9) {

            continue;
        }

        blockDirections.push_back(blockSum.normalized());
    }

    if (blockDirections.size() < minimumBlocks) {

        result.message = "Too few valid stationary gravity blocks.";

        return result;
    }

    // Weight blocks equally so a few extra accepted samples cannot dominate.
    Vector3d consensusSum = Vector3d::Zero();

    for (const Vector3d& direction : blockDirections) {

        consensusSum += direction;
    }

    if (consensusSum.norm() < 1e-9) {
        result.message = "Gravity block directions cancelled.";

        return result;
    }

    const Vector3d consensusDirection = consensusSum.normalized();

    std::vector<double> angularDeviations;
    angularDeviations.reserve(blockDirections.size());

    for (const Vector3d& direction : blockDirections) {

        const double dot = std::clamp(direction.dot(consensusDirection), -1.0, 1.0);

        angularDeviations.push_back(std::acos(dot));
    }

    const auto angularSigma =
        calibration_statistics::robustMadSigma(angularDeviations, minimumBlocks);

    if (!angularSigma || !std::isfinite(*angularSigma)) {

        result.message = "Could not estimate gravity angular uncertainty.";

        return result;
    }

    result.valid = true;
    result.direction = consensusDirection;

    result.angularUncertaintyRadians = *angularSigma;

    result.usedBlocks = blockDirections.size();

    result.message = "Block-based gravity estimate accepted.";

    return result;
}

FunctionalRefinementResult SeatedCalibration::refineStaticOffsetFromObservations(
    const Quaterniond& q_BS_static_input,
    const std::vector<CalibrationVectorObservation>& observations) {

    FunctionalRefinementResult result;

    constexpr double EPSILON = 1e-9;
    // Large departures from a valid static result indicate a sign, mapping or
    // observation-geometry failure; accepted Wahba solutions remain unconstrained.
    constexpr double MAXIMUM_CORRECTION_FROM_STATIC_DEGREES = 60.0;
    const double radians_to_degrees = 180.0 / std::acos(-1.0);

    const auto quaternionAngleDegrees = [radians_to_degrees](Quaterniond quaternion) {
        quaternion.normalize();

        // q and -q are equivalent; use the shortest-rotation representation.
        if (quaternion.w() < 0.0) {
            quaternion.coeffs() *= -1.0;
        }

        const double scalar = std::clamp(quaternion.w(), -1.0, 1.0);

        return 2.0 * std::acos(scalar) * radians_to_degrees;
    };

    const auto vectorAngleDegrees =
        [radians_to_degrees](const Vector3d& first, const Vector3d& second, bool sign_ambiguous) {
            double dot = first.normalized().dot(second.normalized());

            if (sign_ambiguous) {
                dot = std::abs(dot);
            }

            dot = std::clamp(dot, -1.0, 1.0);

            return std::acos(dot) * radians_to_degrees;
        };

    if (!q_BS_static_input.coeffs().allFinite() || q_BS_static_input.squaredNorm() < EPSILON) {

        result.message = "Invalid original static offset.";
        return result;
    }

    if (observations.empty()) {
        result.message = "No functional or gravity observations are available.";
        return result;
    }

    const Quaterniond q_BS_static = q_BS_static_input.normalized();

    const Matrix3d R_BS_static = q_BS_static.toRotationMatrix();

    Matrix3d attitude_profile = Matrix3d::Zero();

    const auto addObservation = [&attitude_profile](const Vector3d& sensor_vector,
                                                    const Vector3d& target_vector, double weight) {
        if (weight <= 0.0) {
            return;
        }

        const Vector3d sensor = sensor_vector.normalized();

        const Vector3d target = target_vector.normalized();

        // Wahba attitude-profile matrix mapping S to B.
        attitude_profile.noalias() += weight * target * sensor.transpose();
    };

    std::vector<CalibrationVectorObservation> accepted_observations;
    accepted_observations.reserve(observations.size());

    for (const CalibrationVectorObservation& input : observations) {
        if (!input.sensorVectorS.allFinite() || !input.targetVectorB.allFinite() ||
            input.sensorVectorS.norm() < EPSILON || input.targetVectorB.norm() < EPSILON ||
            input.weight <= 0.0) {

            continue;
        }

        CalibrationVectorObservation accepted = input;

        accepted.sensorVectorS.normalize();
        accepted.targetVectorB.normalize();

        // Resolve PCA and gravity sign ambiguity against the trusted static offset.
        if (accepted.signAmbiguous &&
            (R_BS_static * accepted.sensorVectorS).dot(accepted.targetVectorB) < 0.0) {

            accepted.sensorVectorS = -accepted.sensorVectorS;
        }

        addObservation(accepted.sensorVectorS, accepted.targetVectorB, accepted.weight);

        accepted_observations.push_back(accepted);
    }

    if (accepted_observations.empty()) {
        result.message = "No valid functional or gravity observations remained.";
        return result;
    }

    // Orthogonal Procrustes: A = U Sigma V^T; R = U diag(1,1,det(UV^T)) V^T.
    JacobiSVD<Matrix3d> svd(attitude_profile, ComputeFullU | ComputeFullV);

    if (svd.info() != Success) {
        result.message = "Multi-observation Wahba SVD failed.";
        return result;
    }

    const Matrix3d U = svd.matrixU();
    const Matrix3d V = svd.matrixV();

    Matrix3d determinant_correction = Matrix3d::Identity();

    determinant_correction(2, 2) = (U * V.transpose()).determinant() < 0.0 ? -1.0 : 1.0;

    const Matrix3d R_BS_wahba = U * determinant_correction * V.transpose();

    if (!R_BS_wahba.allFinite() || std::abs(R_BS_wahba.determinant() - 1.0) > 1e-6) {

        result.message = "Multi-observation Wahba fit produced an invalid rotation.";
        return result;
    }

    const Quaterniond q_BS_wahba = Quaterniond(R_BS_wahba).normalized();

    Quaterniond q_delta = (q_BS_wahba * q_BS_static.conjugate()).normalized();

    if (q_delta.w() < 0.0) {
        q_delta.coeffs() *= -1.0;
    }

    result.correctionDegrees = quaternionAngleDegrees(q_delta);

    if (result.correctionDegrees > MAXIMUM_CORRECTION_FROM_STATIC_DEGREES) {

        result.offset = q_BS_static;
        result.message =
            "Independent Wahba solution is more than 60 degrees from static calibration.";
        return result;
    }

    // Static calibration is used only for sign disambiguation and the safety check.
    result.offset = q_BS_wahba;

    double weighted_squared_error = 0.0;
    double residual_weight = 0.0;
    double maximum_residual = 0.0;

    for (const CalibrationVectorObservation& observation : accepted_observations) {

        const Vector3d fitted_target_B = result.offset * observation.sensorVectorS;

        const double residual_degrees = vectorAngleDegrees(
            fitted_target_B, observation.targetVectorB, observation.signAmbiguous);

        weighted_squared_error += observation.weight * residual_degrees * residual_degrees;

        residual_weight += observation.weight;

        maximum_residual = (std::max)(maximum_residual, residual_degrees);
    }

    result.observationCount = accepted_observations.size();

    result.maximumResidualDegrees = maximum_residual;

    if (residual_weight > EPSILON) {
        result.weightedRmsResidualDegrees = std::sqrt(weighted_squared_error / residual_weight);
    }

    result.valid = true;
    result.message = "Independent multi-observation Wahba result accepted in full.";

    return result;
}

Quaterniond SeatedCalibration::applyCalibration(const Quaterniond& q_sensor_live,
                                                const Quaterniond& q_BS) {
    // q_CB = q_CS * q_SB, and q_SB is conjugate(q_BS).
    return (q_sensor_live.normalized() * q_BS.normalized().conjugate()).normalized();
}
