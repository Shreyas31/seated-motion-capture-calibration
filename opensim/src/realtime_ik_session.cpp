#include "realtime_ik_session.h"

#include "csv_schemas.h"
#include "pose_preset.h"
#include "segment_model_map.h"

#include <OpenSim/OpenSim.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace SeatedMoCap {
namespace {

using ModelFrameCorrections = std::array<SimTK::Rotation, Realtime::kSensorCount>;

double orientationWeight(const std::string& segment) {
    if (segment == "Pelvis" || segment == "Sternum") {
        return 5.0;
    }

    if (segment == "Right_Upperleg" || segment == "Left_Upperleg" || segment == "Right_Lowerleg" ||
        segment == "Left_Lowerleg") {
        return 3.0;
    }

    if (segment == "Head" || segment == "Right_Upperarm" || segment == "Left_Upperarm" ||
        segment == "Right_Forearm" || segment == "Left_Forearm" || segment == "Right_Foot" ||
        segment == "Left_Foot") {
        return 2.0;
    }

    return 1.0;
}

SimTK::Rotation rotationFromWxyz(const std::array<double, 4>& values) {
    return SimTK::Rotation{SimTK::Quaternion{values[0], values[1], values[2], values[3]}};
}

void validateProtocolMappingOrder() {
    if (Realtime::kSensorCount != kSegmentModelMappings.size()) {
        throw std::runtime_error("UDP and OpenSim mapping sizes differ.");
    }

    for (std::size_t index = 0; index < Realtime::kSensorCount; ++index) {
        const std::string protocolSegment{Realtime::kSegmentOrder[index]};
        const std::string modelSegment{kSegmentModelMappings[index].backendSegment};

        if (protocolSegment != modelSegment) {
            throw std::runtime_error("UDP/OpenSim mapping mismatch at index " +
                                     std::to_string(index) + ": UDP='" + protocolSegment +
                                     "', OpenSim='" + modelSegment + "'.");
        }
    }
}

ModelFrameCorrections createModelFrameCorrections(const PosePreset& preset) {
    ModelFrameCorrections corrections{};

    for (std::size_t index = 0; index < Realtime::kSensorCount; ++index) {
        const std::string frameName{kSegmentModelMappings[index].imuFrame};
        const auto ideal = preset.idealOrientationsWxyz.find(frameName);
        const auto model = preset.modelReferenceOrientationsWxyz.find(frameName);

        if (ideal == preset.idealOrientationsWxyz.end() ||
            model == preset.modelReferenceOrientationsWxyz.end()) {
            throw std::runtime_error("Pose preset is missing frame-alignment data for " +
                                     frameName);
        }

        // R_BF = inverse(R_OB_reference) * R_OF_reference.
        corrections[index] = (~rotationFromWxyz(ideal->second)) * rotationFromWxyz(model->second);
    }

    return corrections;
}

OpenSim::TimeSeriesTable_<SimTK::Rotation>
createInitialOrientationTable(const PosePreset& preset,
                              OpenSim::Set<OpenSim::OrientationWeight>& orientationWeights) {
    OpenSim::TimeSeriesTable_<SimTK::Rotation> table;
    std::vector<std::string> labels;
    std::vector<SimTK::Rotation> values;
    labels.reserve(kSegmentModelMappings.size());
    values.reserve(kSegmentModelMappings.size());

    for (const auto& mapping : kSegmentModelMappings) {
        const std::string frameName{mapping.imuFrame};
        const auto orientation = preset.modelReferenceOrientationsWxyz.find(frameName);
        if (orientation == preset.modelReferenceOrientationsWxyz.end()) {
            throw std::runtime_error("Preset is missing model frame: " + frameName);
        }

        labels.push_back(frameName);
        values.push_back(rotationFromWxyz(orientation->second));
        orientationWeights.adoptAndAppend(new OpenSim::OrientationWeight{
            frameName, orientationWeight(std::string{mapping.backendSegment})});
    }

    table.setColumnLabels(labels);
    table.appendRow(0.0, values);
    return table;
}

SimTK::RowVector_<SimTK::Rotation>
orientationRowFromFrame(const Realtime::OrientationFrameV1& frame,
                        const ModelFrameCorrections& corrections) {
    SimTK::RowVector_<SimTK::Rotation> row{static_cast<int>(Realtime::kSensorCount)};

    for (std::size_t index = 0; index < Realtime::kSensorCount; ++index) {
        const auto& source = frame.orientations[index];
        const double norm = std::sqrt(source.w * source.w + source.x * source.x +
                                      source.y * source.y + source.z * source.z);
        const SimTK::Quaternion quaternion{source.w / norm, source.x / norm, source.y / norm,
                                           source.z / norm};
        row[static_cast<int>(index)] = SimTK::Rotation{quaternion} * corrections[index];
    }

    return row;
}

class JointAngleCsvWriter {
  public:
    JointAngleCsvWriter(const OpenSim::Model& model, const std::filesystem::path& outputPath)
        : outputPath_{outputPath} {
        const auto parent = outputPath_.parent_path();

        if (!parent.empty()) {
            std::filesystem::create_directories(parent);
        }

        output_.open(outputPath_, std::ios::out | std::ios::trunc);

        if (!output_) {
            throw std::runtime_error("Could not create joint-angle CSV: " + outputPath_.string());
        }

        output_ << CsvSchema::kOpenSimJointAnglePrefix;

        const auto& coordinates = model.getCoordinateSet();

        for (int index = 0; index < coordinates.getSize(); ++index) {
            const auto& coordinate = coordinates.get(index);

            if (coordinate.getMotionType() != OpenSim::Coordinate::MotionType::Rotational) {
                continue;
            }

            coordinateNames_.push_back(coordinate.getName());
            output_ << ',' << coordinate.getName() << "_deg";
        }

        output_ << '\n';
        output_ << std::setprecision(15);
    }

    bool writeFrame(const OpenSim::Model& model, const SimTK::State& state,
                    const std::uint64_t sequence, const std::int64_t timestampMicroseconds) {
        if (!output_) {
            return false;
        }

        constexpr double radiansToDegrees = 180.0 / 3.14159265358979323846;

        output_ << sequence << ',' << timestampMicroseconds << ',' << state.getTime();

        const auto& coordinates = model.getCoordinateSet();

        for (const auto& name : coordinateNames_) {
            const auto& coordinate = coordinates.get(name);
            output_ << ',' << coordinate.getValue(state) * radiansToDegrees;
        }

        output_ << '\n';

        if (!output_) {
            std::cerr << "Failed while writing joint-angle CSV: " << outputPath_.string()
                      << std::endl;
            return false;
        }

        return true;
    }

  private:
    std::filesystem::path outputPath_;
    std::ofstream output_;
    std::vector<std::string> coordinateNames_;
};

} // namespace

class RealtimeIkSession::Impl {
  public:
    explicit Impl(const RealtimeViewerConfiguration& configuration) {
        OpenSim::ModelVisualizer::addDirToGeometrySearchPaths(configuration.geometryPath.string());
        preset_ = loadPosePreset(configuration.presetPath, configuration.poseName);

        if (configuration.modelPath.filename().string() != preset_.modelFile) {
            throw std::runtime_error("Preset expects model '" + preset_.modelFile +
                                     "', but received '" +
                                     configuration.modelPath.filename().string() + "'.");
        }

        validateProtocolMappingOrder();
        frameCorrections_ = createModelFrameCorrections(preset_);
        model_ = std::make_unique<OpenSim::Model>(configuration.modelPath.string());
        model_->setUseVisualizer(true);
        model_->finalizeConnections();
        state_ = &model_->initSystem();
        model_->updVisualizer().updSimbodyVisualizer().setShutdownWhenDestructed(true);

        applyPosePreset(*model_, *state_, preset_);
        pelvisVerticalCorrection_ = alignFeetToSupportPlane(*model_, *state_, 0.0);

        if (verifyModelReferenceOrientations(*model_, *state_, preset_) > 0.01) {
            throw std::runtime_error(
                "Initial pose does not match its model reference orientations.");
        }

        const auto initialTable = createInitialOrientationTable(preset_, orientationWeights_);
        markerReference_ = std::make_shared<OpenSim::MarkersReference>();
        orientationReference_ = std::make_shared<OpenSim::BufferedOrientationsReference>(
            initialTable, &orientationWeights_);
        solver_ = std::make_unique<OpenSim::InverseKinematicsSolver>(
            *model_, markerReference_, orientationReference_, coordinateReferences_,
            SimTK::Infinity);
        solver_->setAccuracy(1e-5);
        solver_->assemble(*state_);

        if (!configuration.jointAngleOutputPath.empty()) {
            jointAngleWriter_.emplace(*model_, configuration.jointAngleOutputPath);
        }
        if (configuration.stationaryFeetEnabled()) {
            stationaryFootAnchor_.emplace(*model_, *state_);
        }

        sensorsInUse_ = solver_->getNumOrientationSensorsInUse();
        if (sensorsInUse_ != static_cast<int>(kSegmentModelMappings.size())) {
            throw std::runtime_error("IK is using " + std::to_string(sensorsInUse_) +
                                     " orientation sensors; expected " +
                                     std::to_string(kSegmentModelMappings.size()) + ".");
        }
        model_->realizePosition(*state_);
    }

    ~Impl() {
        finishOrientationStream();
    }

    [[nodiscard]] std::optional<FootAnchorUpdate>
    trackFrame(const Realtime::OrientationFrameV1& frame, const double streamTime) {
        orientationReference_->putValues(streamTime,
                                         orientationRowFromFrame(frame, frameCorrections_));
        state_->setTime(streamTime);
        solver_->track(*state_);
        model_->realizePosition(*state_);

        std::optional<FootAnchorUpdate> anchorUpdate;
        if (stationaryFootAnchor_) {
            anchorUpdate = stationaryFootAnchor_->apply(*model_, *state_);
        }

        if (jointAngleWriter_ && !jointAngleWriter_->writeFrame(*model_, *state_, frame.sequence,
                                                                frame.timestampMicroseconds)) {
            throw std::runtime_error("Joint-angle CSV writing failed.");
        }
        return anchorUpdate;
    }

    void showCurrentState() {
        model_->updVisualizer().show(*state_);
    }

    void finishOrientationStream() {
        if (!streamFinished_ && orientationReference_) {
            orientationReference_->setFinished(true);
            streamFinished_ = true;
        }
    }

    PosePreset preset_;
    ModelFrameCorrections frameCorrections_{};
    std::unique_ptr<OpenSim::Model> model_;
    SimTK::State* state_ = nullptr;
    OpenSim::Set<OpenSim::OrientationWeight> orientationWeights_;
    std::shared_ptr<OpenSim::MarkersReference> markerReference_;
    std::shared_ptr<OpenSim::BufferedOrientationsReference> orientationReference_;
    SimTK::Array_<OpenSim::CoordinateReference> coordinateReferences_;
    std::unique_ptr<OpenSim::InverseKinematicsSolver> solver_;
    std::optional<StationaryFootAnchor> stationaryFootAnchor_;
    std::optional<JointAngleCsvWriter> jointAngleWriter_;
    double pelvisVerticalCorrection_ = 0.0;
    int sensorsInUse_ = 0;
    bool streamFinished_ = false;
};

RealtimeIkSession::RealtimeIkSession(const RealtimeViewerConfiguration& configuration)
    : implementation_{std::make_unique<Impl>(configuration)} {}

RealtimeIkSession::~RealtimeIkSession() = default;
RealtimeIkSession::RealtimeIkSession(RealtimeIkSession&&) noexcept = default;
RealtimeIkSession& RealtimeIkSession::operator=(RealtimeIkSession&&) noexcept = default;

double RealtimeIkSession::initialPelvisVerticalCorrection() const noexcept {
    return implementation_->pelvisVerticalCorrection_;
}

int RealtimeIkSession::orientationSensorsInUse() const noexcept {
    return implementation_->sensorsInUse_;
}

std::optional<FootAnchorUpdate>
RealtimeIkSession::trackFrame(const Realtime::OrientationFrameV1& frame, const double streamTime) {
    return implementation_->trackFrame(frame, streamTime);
}

void RealtimeIkSession::showCurrentState() {
    implementation_->showCurrentState();
}

void RealtimeIkSession::finishOrientationStream() {
    implementation_->finishOrientationStream();
}

} // namespace SeatedMoCap
