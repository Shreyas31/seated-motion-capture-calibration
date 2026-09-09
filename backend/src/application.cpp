#include "application.h"

#include "acquisition_workflow.h"
#include "application_bootstrap.h"
#include "awinda_system.h"
#include "frontend_protocol.h"

#include <filesystem>
#include <iostream>

int Application::run(const std::filesystem::path& sensorMappingPath) {
    frontend_protocol::EventWriter frontend{std::cout};
    ApplicationBootstrap bootstrap{frontend, std::cin, std::cout, std::cerr};
    ApplicationBootstrapResult startup = bootstrap.prepare(sensorMappingPath);
    AcquisitionWorkflow acquisition{frontend, std::cin, std::cout, std::cerr};
    return acquisition.run(*startup.awinda, startup.sensorMapping, startup.session);
}
