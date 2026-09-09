cmake_minimum_required(VERSION 3.16)

# The user can supply this through:
# 1. The OPENSIM_INSTALL_DIR CMake cache variable, or
# 2. The OPENSIM_HOME Windows environment variable.
set(
    OPENSIM_INSTALL_DIR
    "$ENV{OPENSIM_HOME}"
    CACHE PATH
    "OpenSim installation directory"
)

if(NOT OPENSIM_INSTALL_DIR)
    message(FATAL_ERROR
        "OpenSim installation directory was not provided. "
        "Set OPENSIM_HOME or pass -DOPENSIM_INSTALL_DIR=<path>."
    )
endif()

# OpenSimConfig.cmake is located here in the Windows OpenSim SDK.
set(
    OpenSim_DIR
    "${OPENSIM_INSTALL_DIR}/sdk/cmake"
    CACHE PATH
    "Directory containing OpenSimConfig.cmake"
)

message(STATUS "OpenSim installation: ${OPENSIM_INSTALL_DIR}")
message(STATUS "OpenSim CMake package: ${OpenSim_DIR}")

find_package(OpenSim CONFIG REQUIRED)

# Generates the project's augmented OpenSim model.
add_executable(generate_seated_mocap_model
    src/generate_seated_mocap_model.cpp
    src/patient_anthropometry.cpp
    src/upper_body_augmentation.cpp
    src/ik_coordinate_policy.cpp
    src/anatomical_frame_alignment.cpp
    src/support_plane_alignment.cpp
)

target_link_libraries(generate_seated_mocap_model PRIVATE
    project_nlohmann_json
)

add_executable(validate_seated_mocap_model
    src/validate_seated_mocap_model.cpp
)

add_executable(generate_pose_presets
    src/generate_pose_presets.cpp
    src/pose_preset.cpp
    ${PROJECT_SOURCE_DIR}/backend/src/seated_pose_targets.cpp
)

target_include_directories(generate_pose_presets
    PRIVATE
    ${PROJECT_SOURCE_DIR}/backend/include
)

target_link_libraries(generate_pose_presets PRIVATE
    project_eigen
    project_nlohmann_json
)

add_executable(opensim_rt_viewer
    src/opensim_rt_viewer.cpp
    src/orientation_stream.cpp
    src/realtime_ik_session.cpp
    src/realtime_viewer_configuration.cpp
    src/realtime_viewer_runner.cpp
    src/udp_orientation_receiver.cpp
    src/pose_preset.cpp
    src/support_plane_alignment.cpp
)

target_include_directories(opensim_rt_viewer
    PRIVATE
    ${PROJECT_SOURCE_DIR}/shared/include
)

target_link_libraries(opensim_rt_viewer
    PRIVATE
    project_nlohmann_json
    ws2_32
)

# Test for coordinate conventions.
add_executable(test_opensim_runtime
    tests/test_opensim_runtime.cpp
)

add_executable(test_coordinate_conventions
    tests/test_coordinate_conventions.cpp
)

add_executable(test_segment_model_map
    tests/test_segment_model_map.cpp
)

add_executable(test_realtime_viewer_configuration
    tests/test_realtime_viewer_configuration.cpp
    src/realtime_viewer_configuration.cpp
)

add_executable(test_realtime_orientation_validation
    tests/test_realtime_orientation_validation.cpp
    src/orientation_stream.cpp
)

add_executable(test_realtime_frame_sequence
    tests/test_realtime_frame_sequence.cpp
    src/orientation_stream.cpp
)

target_include_directories(test_realtime_orientation_validation
    PRIVATE
    ${PROJECT_SOURCE_DIR}/shared/include
)

target_include_directories(test_realtime_frame_sequence
    PRIVATE
    ${PROJECT_SOURCE_DIR}/shared/include
)

if(BUILD_TESTING)
    add_test(
        NAME opensim_runtime
        COMMAND test_opensim_runtime
    )

    add_test(
        NAME coordinate_conventions
        COMMAND test_coordinate_conventions
    )

    add_test(
        NAME segment_model_map
        COMMAND test_segment_model_map
    )

    add_test(
        NAME realtime_viewer_configuration
        COMMAND test_realtime_viewer_configuration
    )

    add_test(
        NAME realtime_orientation_validation
        COMMAND test_realtime_orientation_validation
    )

    add_test(
        NAME realtime_frame_sequence
        COMMAND test_realtime_frame_sequence
    )
endif()

foreach(target_name
        generate_seated_mocap_model
        validate_seated_mocap_model
        generate_pose_presets
        opensim_rt_viewer
        test_opensim_runtime
        test_coordinate_conventions
        test_segment_model_map
        test_realtime_viewer_configuration
        test_realtime_orientation_validation
        test_realtime_frame_sequence)

    set_target_properties(${target_name} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
    )

    target_include_directories(${target_name}
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(${target_name}
        PRIVATE
        ${OpenSim_LIBRARIES}
    )
endforeach()

install(
    TARGETS
        generate_seated_mocap_model
        opensim_rt_viewer
    RUNTIME DESTINATION bin
)
