# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required(VERSION 3.16)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project(WeightedEstimate_Seated_Calibration_IRP LANGUAGES CXX)

include(CTest)

# Third-party headers vendored with the repository. Components link these
# targets instead of reaching into one another's source trees.
add_library(project_eigen INTERFACE)
target_include_directories(project_eigen SYSTEM INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/dependencies/Eigen>"
)

add_library(project_nlohmann_json INTERFACE)
target_include_directories(project_nlohmann_json SYSTEM INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/dependencies>"
)

# Include sub-projects.
# The existing Xsens backend.
add_subdirectory("backend")

# Keep OpenSim targets out of the ordinary Debug backend build unless enabled
# by the selected CMake preset.
option(
    BUILD_OPENSIM_COMPONENTS
    "Build the OpenSim model tools and real-time viewer"
    OFF
)

if(BUILD_OPENSIM_COMPONENTS)
    add_subdirectory("opensim")
endif()

install(
    FILES
         "${CMAKE_SOURCE_DIR}/frontend/__init__.py"
         "${CMAKE_SOURCE_DIR}/frontend/app.py"
         "${CMAKE_SOURCE_DIR}/frontend/app_config.py"
         "${CMAKE_SOURCE_DIR}/frontend/application_bootstrap.py"
         "${CMAKE_SOURCE_DIR}/frontend/calibration_page.py"
         "${CMAKE_SOURCE_DIR}/frontend/events_controller.py"
         "${CMAKE_SOURCE_DIR}/frontend/execution_controller.py"
         "${CMAKE_SOURCE_DIR}/frontend/setup_page.py"
         "${CMAKE_SOURCE_DIR}/frontend/presentation_controller.py"
         "${CMAKE_SOURCE_DIR}/frontend/process_supervisor.py"
         "${CMAKE_SOURCE_DIR}/frontend/protocol.py"
         "${CMAKE_SOURCE_DIR}/frontend/recording_page.py"
         "${CMAKE_SOURCE_DIR}/frontend/runtime_context.py"
         "${CMAKE_SOURCE_DIR}/frontend/sensor_mapping.py"
         "${CMAKE_SOURCE_DIR}/frontend/session_files.py"
         "${CMAKE_SOURCE_DIR}/frontend/session_state.py"
         "${CMAKE_SOURCE_DIR}/frontend/shutdown_controller.py"
         "${CMAKE_SOURCE_DIR}/frontend/system_check.py"
         "${CMAKE_SOURCE_DIR}/frontend/workflow_controller.py"
         "${CMAKE_SOURCE_DIR}/frontend/workflow_ui.py"
         "${CMAKE_SOURCE_DIR}/frontend/requirements.txt"
        "${CMAKE_SOURCE_DIR}/frontend/style.qss"
        "${CMAKE_SOURCE_DIR}/frontend/style_dark.qss"
    DESTINATION frontend
)

install(
    FILES
        "${CMAKE_SOURCE_DIR}/generated/pose_presets.json"
    DESTINATION resources
)

install(
    FILES
        "${CMAKE_SOURCE_DIR}/config/sensor_mapping.json"
    DESTINATION config
)

install(
    FILES
        "${CMAKE_SOURCE_DIR}/deployment/setup_frontend.cmd"
        "${CMAKE_SOURCE_DIR}/deployment/launch_frontend.cmd"
        "${CMAKE_SOURCE_DIR}/deployment/README.md"
    DESTINATION .
)
