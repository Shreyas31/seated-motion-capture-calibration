cmake_minimum_required(VERSION 3.16)
project(SeatedMocapBackend LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

set(XSENS_DIR "${CMAKE_SOURCE_DIR}/dependencies/xsens")

if(WIN32)
    set(XDA_LIBS
        "${XSENS_DIR}/lib/xsensdeviceapi64.lib"
        "${XSENS_DIR}/lib/xstypes64.lib"
    )

    file(GLOB XDA_DLLS "${XSENS_DIR}/lib/*.dll")
endif()

find_package(Threads REQUIRED)

set(BACKEND_CORE_SOURCES
    src/acquisition_workflow.cpp
    src/angular_uncertainty.cpp
    src/application.cpp
    src/application_bootstrap.cpp
    src/application_menu.cpp
    src/awinda_system.cpp
    src/calibration_csv_exporter.cpp
    src/calibration_sample_analysis.cpp
    src/calibration_session.cpp
    src/frontend_protocol.cpp
    src/functional_calibration_capture.cpp
    src/functional_calibration_solver.cpp
    src/functional_calibration_workflow.cpp
    src/functional_return_pose.cpp
    src/heading_offset_workflow.cpp
    src/measurement_csv_writer.cpp
    src/measurement_recording_workflow.cpp
    src/output_naming.cpp
    src/packet_collector.cpp
    src/runtime_configuration.cpp
    src/seated_calibration.cpp
    src/seated_pose_targets.cpp
    src/sensor_connection_workflow.cpp
    src/sensor_mapping.cpp
    src/session_setup_workflow.cpp
    src/static_calibration_workflow.cpp
    src/udp_orientation_streamer.cpp
    src/xsens_packet_utils.cpp
)

add_library(backend_core STATIC ${BACKEND_CORE_SOURCES})

target_include_directories(backend_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/shared/include"
)

target_include_directories(backend_core SYSTEM PUBLIC
    "${XSENS_DIR}/include"
)

target_link_libraries(backend_core PUBLIC
    project_eigen
    ${XDA_LIBS}
    Threads::Threads
    ws2_32
)

target_link_libraries(backend_core PRIVATE
    project_nlohmann_json
)

add_executable(backend src/main.cpp)
target_link_libraries(backend PRIVATE backend_core)

target_compile_definitions(backend PRIVATE
    SEATED_MOCAP_DEFAULT_SENSOR_MAP="${CMAKE_SOURCE_DIR}/config/sensor_mapping.json"
)

function(copy_xsens_runtime target_name)
    if(WIN32)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${XDA_DLLS}
            $<TARGET_FILE_DIR:${target_name}>
        )
    endif()
endfunction()

copy_xsens_runtime(backend)

install(
    TARGETS backend
    RUNTIME DESTINATION bin
)

if(WIN32)
    install(
        DIRECTORY "${XSENS_DIR}/lib/"
        DESTINATION bin
        FILES_MATCHING
        PATTERN "*.dll"
    )
endif()

function(register_backend_test target_name)
    add_executable(${target_name} "tests/${target_name}.cpp")
    target_link_libraries(${target_name} PRIVATE backend_core)
    copy_xsens_runtime(${target_name})

    string(REGEX REPLACE "^test_" "" test_name "${target_name}")
    add_test(NAME ${test_name} COMMAND ${target_name})
endfunction()

if(BUILD_TESTING)
    set(BACKEND_TEST_TARGETS
        test_angular_uncertainty
        test_application_menu
        test_backend_core
        test_calibration_sample_analysis
        test_data_contracts
        test_frontend_protocol
        test_functional_axis_targets
        test_functional_calibration_capture
        test_functional_calibration_selection
        test_functional_observations
        test_heading_offset_service
        test_heading_offset_workflow
        test_recording_stop_input
        test_return_pose_decision
        test_runtime_configuration
        test_sensor_connection_status
        test_session_setup_workflow
        test_static_pose_selection
    )

    foreach(test_target IN LISTS BACKEND_TEST_TARGETS)
        register_backend_test(${test_target})
    endforeach()

    target_include_directories(test_data_contracts PRIVATE
        "${CMAKE_SOURCE_DIR}/opensim/include"
    )
endif()
