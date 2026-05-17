cmake_minimum_required(VERSION 3.20)
project(WinSCADA LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find ONNX Runtime (placeholder for now, will need actual path in production)
# find_package(ONNXRuntime REQUIRED)

add_subdirectory(src/shared)
add_subdirectory(src/agent)
add_subdirectory(src/watchdog)
