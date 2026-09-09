#pragma once

#include <Eigen>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

/**
 * Result of fitting a line through angular-velocity samples.
 * The axis is expressed in the same coordinate system as the input samples.
 * PCA cannot determine the sign of an axis; that ambiguity is resolved later using the static sensor-to-segment estimate.
 * @struct FunctionalAxisEstimate.
 */
struct FunctionalAxisEstimate {
    bool valid = false;
    Eigen::Vector3d axis = Eigen::Vector3d::UnitY();
    double confidence = 0.0;
    double rmsAngularSpeed = 0.0;
    // Samples remaining after the speed threshold.
    std::size_t candidateSamples = 0;

    // Candidate samples rejected for pointing too far from the provisional axis.
    std::size_t rejectedOutliers = 0;

    // Samples used by the final PCA.
    std::size_t usedSamples = 0;

    // Fraction of speed-qualified samples retained as directional inliers.
    double inlierFraction = 0.0;

    // Robust angular variation between independently estimated movement
    // blocks or cycles. Stored in radians for use in variance-based weights.
    double angularUncertaintyRadians = 0.0;

    // Number of valid block/cycle PCA estimates used to calculate uncertainty.
    std::size_t uncertaintyBlocks = 0;

    std::string message;
};

/**
 * Result of estimating gravity direction from stationary acceleration
 * blocks.
 */
struct GravityDirectionEstimate {
    bool valid = false;

    // Consensus gravity/specific-force direction in sensor frame S.
    Eigen::Vector3d direction = Eigen::Vector3d::Zero();

    // Robust angular variation between block directions, in radians.
    double angularUncertaintyRadians = 0.0;

    // Number of valid stationary blocks used.
    std::size_t usedBlocks = 0;

    std::string message;
};

/**
 * One measured vector correspondence used by the multi-observation Wahba calibration.
 * The solver attempts to satisfy: targetVectorB approximately equals q_BS * sensorVectorS.
 * @struct CalibrationVectorObservation.
 */
struct CalibrationVectorObservation {
    Eigen::Vector3d sensorVectorS = Eigen::Vector3d::UnitX();
    Eigen::Vector3d targetVectorB = Eigen::Vector3d::UnitX();

    // Relative authority of this observation in the Wahba objective.
    double weight = 1.0;

    // Functional PCA axes are unoriented lines. Gravity may also require a
    // sign correction depending on the accelerometer convention. When true,
    // the solver chooses the sign that agrees with the static prior.
    bool signAmbiguous = false;

    // Used for diagnostics and for replacing a repeated joint capture.
    std::string source;

    // Angular uncertainty used to derive this observation's Wahba weight.
    // Stored for diagnostics and reproducibility.
    double angularUncertaintyRadians = 0.0;
};

/**
 * Result of refining a complete static sensor-to-segment rotation.
 * @struct FunctionalRefinementResult.
 */
struct FunctionalRefinementResult {
    bool valid = false;

    // Final sensor-to-body rotation q_BS.
    Eigen::Quaterniond offset = Eigen::Quaterniond::Identity();

    // Complete three-dimensional rotation from the immutable static offset to
    // the accepted independent Wahba solution. No partial correction is used.
    double correctionDegrees = 0.0;

    // Residual across every non-static vector observation supplied to the fit.
    double weightedRmsResidualDegrees = 0.0;
    double maximumResidualDegrees = 0.0;
    std::size_t observationCount = 0;

    std::string message;
};

/**
 * Performs seated calibration of IMU sensors using static and functional calibration methods.
 * @class SeatedCalibration.
 */
class SeatedCalibration {
  public:
    /**
     * Computes the offset quaternion needed to align the sensor quaternion with the predefined body pose for static calibration.
     * @param q_GS Sensor-to-global orientation quaternion.
	 * @param q_GB Body-to-global target orientation quaternion.
	 * @return Normalized sensor-to-body offset quaternion q_BS.
	 */
    static Eigen::Quaterniond computeStaticOffset(const Eigen::Quaterniond& q_CS,
                                                  const Eigen::Quaterniond& q_CB);

    /**
	* Computes a sign-ambiguous functional axis using a line fit through the origin.
	* A static gyro bias is removed first. Low-speed samples are discarded, then
	* the dominant eigenvector of sum(omega * omega^T) is used.
	* @param angular_velocities Angular velocities in a single coordinate frame.
	* @param gyro_bias Bias measured while the sensor was stationary.
	* @param minimum_speed Samples below this speed in rad/s are ignored.
	* @return Axis estimate, confidence, sample-quality metrics, and diagnostic message.
	*/
    static FunctionalAxisEstimate
    computeFunctionalAxis(const std::vector<Eigen::Vector3d>& angular_velocities,
                          const Eigen::Vector3d& gyro_bias = Eigen::Vector3d::Zero(),
                          double minimum_speed = 0.35);

    // Estimates the functional axis from the complete capture, then measures how
    // consistently that axis is reproduced in several chronological blocks.
    //
    // The complete-capture PCA remains the final axis estimate. The block estimates
    // are used only to calculate angular uncertainty.
    static FunctionalAxisEstimate computeFunctionalAxisWithUncertainty(
        const std::vector<Eigen::Vector3d>& angular_velocities,
        const Eigen::Vector3d& gyro_bias = Eigen::Vector3d::Zero(), double minimum_speed = 0.35,
        std::size_t requested_block_count = 5, std::size_t minimum_valid_blocks = 4);

    /**
	* Computes a Markley quaternion mean.
	* q and -q encode the same orientation. The outer-product formulation avoids
	* cancellation across that sign boundary.
	* @param quaternions Orientation samples to average.
	* @return Normalized mean quaternion, or no value when the input is empty or invalid.
	*/
    static std::optional<Eigen::Quaterniond>
    averageQuaternions(const std::vector<Eigen::Quaterniond>& quaternions);

    /**
     * Defines a calibration-relative global frame from one or more sensors with a known forward-facing local axis.
     * Each q_GS is an orientation from sensor frame S to earth-fixed NWU frame G.
     * sensor_forward_axis_S specifies the local sensor axis known to point in the patient's forward direction during static calibration.
     *
     * The forward observations are projected onto the NWU horizontal plane and averaged.
     * The returned q_CG maps earth-fixed NWU coordinates G into session coordinates C:
     * +X_C = patient/chair forward
     * +Y_C = patient/chair left
     * +Z_C = up
     *
     * The patient's head must remain neutral and face directly forward during this measurement.
     * @param torso_orientations_GS Sensor-to-global NWU orientation samples q_GS.
	 * @param sensor_forward_axis_S Local sensor axis aligned with patient forward.
	 * @return Global-to-session yaw quaternion q_CG, or no value if heading is indeterminate.
	 */
    static std::optional<Eigen::Quaterniond> computeGlobalToSessionYaw(
        const std::vector<Eigen::Quaterniond>& torso_orientations_GS,
        const Eigen::Vector3d& sensor_forward_axis_S = Eigen::Vector3d::UnitX());

    /**
     * Estimates stationary gravity direction and its block-to-block angular
     * uncertainty.
     *
	 * The current Awinda configuration records at 60 Hz. The default block size
	 * of 30 samples therefore represents approximately 0.5 seconds.
     *
     * @param accelerations Chronological calibrated acceleration samples.
     * @param samplesPerBlock Number of raw samples assigned to each block.
     * @param minimumBlocks Minimum valid blocks required for uncertainty.
     * @return Gravity direction, MAD uncertainty and quality diagnostics.
     */
    static GravityDirectionEstimate
    computeGravityDirection(const std::vector<Eigen::Vector3d>& accelerations,
                            std::size_t samplesPerBlock = 30, std::size_t minimumBlocks = 4);

    /**
     * Recomputes q_BS independently from every functional or gravity observation collected for this sensor.
	 * @param q_BS_static Original static sensor-to-body rotation.
	 * @param observations Functional and gravity vector correspondences for the sensor.
	 * @return Full Wahba offset, residuals in degrees, and calibration-quality diagnostics.
	 */
    static FunctionalRefinementResult refineStaticOffsetFromObservations(
        const Eigen::Quaterniond& q_BS_static,
        const std::vector<CalibrationVectorObservation>& observations);

    /**
     * Applies the final offset quaternion to the live sensor quaternion to obtain a calibrated quaternion.
     * @param q_sensor_live Live sensor-to-session orientation q_CS during recording.
	 * @param q_BS Final sensor-to-body offset after static and functional calibration.
	 * @return Calibrated body-to-session orientation q_CB.
	 */
    static Eigen::Quaterniond applyCalibration(const Eigen::Quaterniond& q_sensor_live,
                                               const Eigen::Quaterniond& q_BS);
};
