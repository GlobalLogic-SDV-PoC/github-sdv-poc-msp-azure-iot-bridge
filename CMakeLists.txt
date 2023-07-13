cmake_minimum_required(VERSION 3.5)
project(
    azure_iot_bridge_core
    LANGUAGES CXX
    VERSION 0.1.0)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

option(AZURE_IOT_BRIDGE_ENABLE_TESTING "Specify if testing should be enabled"
       OFF)
option(AZURE_IOT_BRIDGE_ENABLE_CODE_COVERAGE
       "Specify if code coverage should be enabled" OFF)
option(AZURE_IOT_BRIDGE_EXTERNAL_GTEST
       "Specify whether to use an external googletest library" OFF)
set(LOG_LEVELS
    TRACE
    DEBUG
    INFO
    WARN
    ERROR
    CRITICAL
    OFF)
set(AZURE_IOT_BRIDGE_LOG_LEVEL
    "TRACE"
    CACHE STRING "Specify spdlog SPDLOG_ACTIVE_LEVEL ${LOG_LEVELS}")

if(AZURE_IOT_BRIDGE_ENABLE_TESTING)
    if(AZURE_IOT_BRIDGE_ENABLE_CODE_COVERAGE)
        include(CodeCoverage)
        append_coverage_compiler_flags()
    endif()

    if(NOT AZURE_IOT_BRIDGE_EXTERNAL_GTEST)
        add_subdirectory(dep/external/gtest)
    endif()
endif()

add_subdirectory(dep/internal/iot)
add_subdirectory(dep/external/azure-iot-sdk-c)

set(use_mqtt
    ON
    CACHE BOOL "" FORCE)
set(use_amqp
    OFF
    CACHE BOOL "" FORCE)
set(use_http
    OFF
    CACHE BOOL "" FORCE)
set(build_service_client
    OFF
    CACHE BOOL "" FORCE)
set(build_provisioning_service_client
    OFF
    CACHE BOOL "" FORCE)
set(skip_samples
    ON
    CACHE BOOL "" FORCE)
set(BUILD_TESTING
    OFF
    CACHE BOOL "" FORCE)

add_library(${PROJECT_NAME} STATIC src/client_iot.cpp src/client_iot_impl.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC include)
target_link_libraries(
    ${PROJECT_NAME}
    PUBLIC iot_bridge_core
    PRIVATE iothub_client)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 99)
target_compile_options(${PROJECT_NAME} PRIVATE -Werror -Wall -Wextra)

list(FIND LOG_LEVELS ${AZURE_IOT_BRIDGE_LOG_LEVEL} LOG_LEVEL_INDEX)

if(LOG_LEVEL_INDEX EQUAL -1)
    message(
        FATAL_ERROR
            "Invalid LOG_LEVEL: ${AZURE_IOT_BRIDGE_LOG_LEVEL}. Allowed values are ${LOG_LEVELS}."
    )
endif()

message("Setting LOG_LEVEL to ${AZURE_IOT_BRIDGE_LOG_LEVEL}")
target_compile_definitions(${PROJECT_NAME}
                           PRIVATE SPDLOG_ACTIVE_LEVEL=${LOG_LEVEL_INDEX})

if(AZURE_IOT_BRIDGE_ENABLE_TESTING)
    add_subdirectory(test)
endif()
