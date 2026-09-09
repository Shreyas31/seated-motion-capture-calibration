#include "anatomical_frame_alignment.h"
#include "ik_coordinate_policy.h"
#include "patient_anthropometry.h"
#include "segment_model_map.h"
#include "support_plane_alignment.h"
#include "upper_body_augmentation.h"

#include <OpenSim/OpenSim.h>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

bool containsBody(const OpenSim::Model& model, const std::string& bodyName) {
    const auto& bodies = model.getBodySet();

    for (int index = 0; index < bodies.getSize(); ++index) {
        if (bodies.get(index).getName() == bodyName) {
            return true;
        }
    }

    return false;
}

void validateExistingBodyMappings(const OpenSim::Model& model) {
    for (const auto& mapping : SeatedMoCap::kSegmentModelMappings) {

        const std::string bodyName{mapping.opensimBody};

        if (mapping.treatment == SeatedMoCap::BodyTreatment::AddedByAugmentation) {

            std::cout << "[ADDED] " << mapping.backendSegment << " -> " << bodyName << '\n';

            continue;
        }

        if (!containsBody(model, bodyName)) {
            throw std::runtime_error("Rajagopal body required by mapping is missing: " + bodyName);
        }

        std::cout << "[FOUND]  " << mapping.backendSegment << " -> " << bodyName << '\n';
    }
}

bool bodyContainsFrame(const OpenSim::Body& body, const std::string& frameName) {
    for (const auto& frame : body.getComponentList<OpenSim::PhysicalOffsetFrame>()) {

        if (frame.getName() == frameName) {
            return true;
        }
    }

    return false;
}

void addImuFrame(OpenSim::Model& model, const std::string& bodyName, const std::string& frameName) {
    auto& body = model.updBodySet().get(bodyName);

    if (bodyContainsFrame(body, frameName)) {
        throw std::runtime_error("Body '" + bodyName + "' already contains IMU frame '" +
                                 frameName + "'.");
    }

    auto* imuFrame = new OpenSim::PhysicalOffsetFrame{};

    imuFrame->setName(frameName);
    imuFrame->connectSocket_parent(body);
    imuFrame->set_translation(SimTK::Vec3{0.0});
    imuFrame->set_orientation(SimTK::Vec3{0.0});

    body.addComponent(imuFrame);

    std::cout << "[ADDED]  " << frameName << " -> " << bodyName << '\n';
}

void addExistingBodyImuFrames(OpenSim::Model& model) {
    for (const auto& mapping : SeatedMoCap::kSegmentModelMappings) {

        if (mapping.treatment != SeatedMoCap::BodyTreatment::ExistingRajagopalBody) {
            continue;
        }

        addImuFrame(model, std::string{mapping.opensimBody}, std::string{mapping.imuFrame});
    }
}

OpenSim::ScaleSet createScaleSet(const OpenSim::Model& model,
                                 const SeatedMoCap::AnthropometricScaleFactors& factors) {
    OpenSim::ScaleSet scaleSet;

    for (int index = 0; index < model.getBodySet().getSize(); ++index) {

        const auto& body = model.getBodySet().get(index);
        const std::string& bodyName = body.getName();

        double factor = factors.heightScale;

        // Apply the optional foot factor uniformly to the complete
        // Rajagopal foot chain.
        if (factors.footScale &&
            (bodyName == "talus_r" || bodyName == "calcn_r" || bodyName == "toes_r" ||
             bodyName == "talus_l" || bodyName == "calcn_l" || bodyName == "toes_l")) {

            factor = *factors.footScale;
        }

        auto* scale = new OpenSim::Scale{};

        scale->setSegmentName(bodyName);
        scale->setScaleFactors(SimTK::Vec3{factor, factor, factor});
        scale->setApply(true);

        scaleSet.adoptAndAppend(scale);
    }

    return scaleSet;
}

void scaleModel(OpenSim::Model& model, const SeatedMoCap::PatientAnthropometry& patient,
                const SeatedMoCap::AnthropometricScaleFactors& factors) {
    SimTK::State state = model.initSystem();

    OpenSim::ScaleSet scaleSet = createScaleSet(model, factors);

    // With no measured mass, retain the generic body masses.
    // Inertias are still updated for the changed dimensions.
    const bool preserveMassDistribution = false;

    const double finalMass = patient.massKg.value_or(-1.0);

    if (!model.scale(state, scaleSet, preserveMassDistribution, finalMass)) {

        throw std::runtime_error("OpenSim failed to scale the model.");
    }
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc != 5) {
            std::cerr << "Usage:\n"
                      << "  generate_seated_mocap_model "
                      << "<source-model.osim> "
                      << "<output-model.osim> "
                      << "<geometry-directory> "
                      << "<patient-anthropometry.json>\n";

            return 2;
        }

        const std::filesystem::path sourcePath{argv[1]};
        const std::filesystem::path outputPath{argv[2]};
        const std::filesystem::path geometryPath{argv[3]};
        const std::filesystem::path anthropometryPath{argv[4]};

        if (!std::filesystem::exists(sourcePath)) {
            throw std::runtime_error("Source model does not exist: " + sourcePath.string());
        }

        if (!std::filesystem::exists(geometryPath)) {
            throw std::runtime_error("Geometry directory does not exist: " + geometryPath.string());
        }

        const auto patient = SeatedMoCap::PatientAnthropometry::load(anthropometryPath);

        const auto scaleFactors = SeatedMoCap::calculateScaleFactors(patient);

        std::cout << "Anthropometry:\n"
                  << "  Subject: " << patient.subjectId << '\n'
                  << "  Height: " << patient.height << " m\n"
                  << "  Height scale: " << scaleFactors.heightScale << '\n';

        if (patient.footLength) {
            std::cout << "  Foot length: " << *patient.footLength << " m\n"
                      << "  Foot scale: " << *scaleFactors.footScale << '\n';
        } else {
            std::cout << "  Foot length: not provided; "
                      << "using height scale\n";
        }

        OpenSim::ModelVisualizer::addDirToGeometrySearchPaths(geometryPath.string());

        OpenSim::Model model{sourcePath.string()};
        model.setName("Rajagopal_SeatedMoCap");

        validateExistingBodyMappings(model);
        scaleModel(model, patient, scaleFactors);

        SeatedMoCap::augmentRajagopalUpperBody(model, scaleFactors.heightScale);
        addExistingBodyImuFrames(model);
        const double effectiveFootScale = scaleFactors.footScale.value_or(scaleFactors.heightScale);

        SeatedMoCap::addFootSoleLandmarks(model, effectiveFootScale);
        SeatedMoCap::configureCoordinatesForOrientationIk(model);
        SeatedMoCap::alignAllVirtualImuFrames(model);

        // Let OpenSim validate all model connections before writing the augmented model.
        model.finalizeConnections();
        model.initSystem();

        const std::filesystem::path outputGeometryPath = outputPath.parent_path() / "Geometry";

        std::filesystem::create_directories(outputGeometryPath);

        std::filesystem::copy(geometryPath, outputGeometryPath,
                              std::filesystem::copy_options::recursive |
                                  std::filesystem::copy_options::skip_existing);

        model.print(outputPath.string());

        std::cout << "\nVerified model written to:\n"
                  << std::filesystem::absolute(outputPath).string() << '\n';

        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Model generation failed: " << error.what() << '\n';

        return 1;
    }
}
