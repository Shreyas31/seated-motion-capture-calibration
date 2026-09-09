#include "functional_return_pose.h"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void expect(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

calibration_analysis::ReturnPoseValidationResult validResult(const double referenceError = 5.0,
                                                             const double candidateError = 5.0) {
    return {true, candidateError, referenceError, 50, "Accepted"};
}

void testAcceptanceAndInclusiveLimits() {
    expect(evaluateReturnPose(validResult(), validResult()) == ReturnPoseDecision::Accept,
           "Valid return pose was rejected.");
    expect(evaluateReturnPose(validResult(20.0, 20.0), validResult(20.0, 20.0)) ==
               ReturnPoseDecision::Accept,
           "Values exactly at the clinical limits were rejected.");
}

void testInvalidCaptureTakesPriority() {
    auto invalid = validResult();
    invalid.valid = false;
    expect(evaluateReturnPose(invalid, validResult(30.0, 30.0)) ==
               ReturnPoseDecision::InvalidCapture,
           "Invalid capture did not take priority.");
}

void testReferenceAndCandidateFailuresAreDistinct() {
    expect(evaluateReturnPose(validResult(21.0), validResult()) ==
               ReturnPoseDecision::ReferencePoseNotReproduced,
           "Reference-pose failure was misclassified.");
    expect(evaluateReturnPose(validResult(5.0, 21.0), validResult()) ==
               ReturnPoseDecision::CandidateErrorTooLarge,
           "Candidate-error failure was misclassified.");
}

} // namespace

int main() {
    try {
        testAcceptanceAndInclusiveLimits();
        testInvalidCaptureTakesPriority();
        testReferenceAndCandidateFailuresAreDistinct();
    } catch (const std::exception& error) {
        std::cerr << "Return-pose decision test failed: " << error.what() << std::endl;
        return 1;
    }
    std::cout << "Return-pose decision tests passed." << std::endl;
    return 0;
}
