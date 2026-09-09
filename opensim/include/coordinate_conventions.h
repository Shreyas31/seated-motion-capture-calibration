#pragma once

#include <cmath>
#include <stdexcept>

/**
 * Frame notation:
 * C = backend calibration/session frame
 * O = OpenSim ground frame
 * B = anatomical segment frame
 */
namespace SeatedMoCap::Coordinates {

/**
 * Stores a Cartesian vector without introducing an OpenSim dependency.
 * @struct Vector3.
 */
struct Vector3 {
    double x{};
    double y{};
    double z{};
};

/**
 * Stores a scalar-first quaternion in w, x, y, z order.
 * @struct QuaternionWxyz.
 */
struct QuaternionWxyz {
    double w{1.0};
    double x{};
    double y{};
    double z{};
};

inline constexpr double kSqrtHalf = 0.70710678118654752440;

/**
 * Rotation from backend session frame C into OpenSim ground frame O.
 * Equivalent to -90
 * degrees about +X.
 */
inline constexpr QuaternionWxyz kQuaternionOC{kSqrtHalf, -kSqrtHalf, 0.0, 0.0};

/**
 * Converts vector components from backend session frame C to OpenSim ground frame O.
 * +X_C forward -> +X_O forward
 * +Y_C left    -> -Z_O (because +Z_O is right)
 * +Z_C up      -> +Y_O up
 * @param vectorC Vector expressed in the session frame.
 * @return Equivalent vector expressed in the OpenSim ground frame.
 */
inline constexpr Vector3 sessionVectorToOpenSim(const Vector3& vectorC) {
    return {vectorC.x, vectorC.z, -vectorC.y};
}

/**
 * Computes the Hamilton product of two scalar-first quaternions.
 * @param first Left quaternion representing the outer rotation.
 * @param second Right quaternion applied first under active-rotation composition.
 * @return Quaternion product first multiplied by second without normalization.
 */
inline constexpr QuaternionWxyz multiply(const QuaternionWxyz& first,
                                         const QuaternionWxyz& second) {
    return {first.w * second.w - first.x * second.x - first.y * second.y - first.z * second.z,

            first.w * second.x + first.x * second.w + first.y * second.z - first.z * second.y,

            first.w * second.y - first.x * second.z + first.y * second.w + first.z * second.x,

            first.w * second.z + first.x * second.y - first.y * second.x + first.z * second.w};
}

/**
 * Normalizes a quaternion and canonicalizes it to a non-negative scalar component.
 * @param quaternion Finite non-zero quaternion to normalize.
 * @return Unit quaternion with equivalent rotation and w greater than or equal to zero.
 * @throws std::invalid_argument if the quaternion norm is non-finite or effectively zero.
 */
inline QuaternionWxyz normalized(QuaternionWxyz quaternion) {
    const double norm = std::sqrt(quaternion.w * quaternion.w + quaternion.x * quaternion.x +
                                  quaternion.y * quaternion.y + quaternion.z * quaternion.z);

    if (!std::isfinite(norm) || norm < 1e-12) {
        throw std::invalid_argument("Cannot normalize an invalid quaternion.");
    }

    quaternion.w /= norm;
    quaternion.x /= norm;
    quaternion.y /= norm;
    quaternion.z /= norm;

    if (quaternion.w < 0.0) {
        quaternion.w = -quaternion.w;
        quaternion.x = -quaternion.x;
        quaternion.y = -quaternion.y;
        quaternion.z = -quaternion.z;
    }

    return quaternion;
}

/**
 * Converts body-to-session orientation q_CB into body-to-OpenSim orientation q_OB.
 * @param quaternionCB Quaternion rotating anatomical frame B into session frame C.
 * @return Normalized quaternion rotating anatomical frame B into OpenSim ground frame O.
 * @throws std::invalid_argument if the composed quaternion cannot be normalized.
 */
inline QuaternionWxyz sessionBodyToOpenSim(const QuaternionWxyz& quaternionCB) {
    return normalized(multiply(kQuaternionOC, quaternionCB));
}

} // namespace SeatedMoCap::Coordinates
