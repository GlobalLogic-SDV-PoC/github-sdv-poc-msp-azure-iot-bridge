cmake_minimum_required(VERSION 3.10)
project(azure_iot)

# Default to C++14
if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 14)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(iotb REQUIRED)


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
add_subdirectory(dep/external/azure)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_executable(${PROJECT_NAME} src/main.cpp src/client_iot.cpp
                               src/client_iot_impl.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE include)
target_compile_features(${PROJECT_NAME} PRIVATE c_std_99)
ament_target_dependencies(${PROJECT_NAME} rclcpp std_msgs iotb)
target_link_libraries(${PROJECT_NAME} iothub_client)

install(TARGETS ${PROJECT_NAME} DESTINATION lib/${PROJECT_NAME})

ament_package()
