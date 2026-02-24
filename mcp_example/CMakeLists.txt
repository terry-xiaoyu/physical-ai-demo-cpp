cmake_minimum_required(VERSION 3.14)
project(mcp_server_example VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# macOS: Ensure C++ standard library headers are found
if(APPLE)
    execute_process(
        COMMAND xcrun --show-sdk-path
        OUTPUT_VARIABLE MACOS_SDK_PATH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(MACOS_SDK_PATH)
        set(MACOS_CXX_INCLUDE "${MACOS_SDK_PATH}/usr/include/c++/v1")
        if(EXISTS "${MACOS_CXX_INCLUDE}")
            include_directories(SYSTEM "${MACOS_CXX_INCLUDE}")
        endif()
    endif()
endif()

# Include CPM.cmake
include(../cmake/CPM.cmake)

# Add MCP over MQTT SDK dependency
CPMAddPackage(
    NAME mcp_mqtt_server
    GITHUB_REPOSITORY terry-xiaoyu/mcp-over-mqtt-cpp-sdk
    GIT_TAG main
    OPTIONS
        "BUILD_EXAMPLES OFF"
        "BUILD_SHARED_LIBS OFF"
)

# Optional: Find Paho MQTT C++ for the example
find_package(PahoMqttCpp QUIET)

if(PahoMqttCpp_FOUND)
    add_executable(mcp_server_demo main.cpp)
    target_link_libraries(mcp_server_demo
        PRIVATE
            mcp_mqtt_server
            PahoMqttCpp::paho-mqttpp3
    )
    message(STATUS "Building MCP server demo with Paho MQTT C++")
else()
    message(STATUS "Paho MQTT C++ not found. Install it to build the demo.")
    message(STATUS "See README.md for installation instructions.")
endif()
