# Find ONNX Runtime
find_package(onnxruntime CONFIG QUIET)

if(onnxruntime_FOUND)
    message(STATUS "ONNX Runtime found via CONFIG")
    set(HAS_ONNX TRUE)
else()
    find_path(ONNXRUNTIME_INCLUDE_DIR onnxruntime_cxx_api.h PATH_SUFFIXES onnxruntime)
    find_library(ONNXRUNTIME_LIBRARY onnxruntime)
    if(ONNXRUNTIME_INCLUDE_DIR AND ONNXRUNTIME_LIBRARY)
        message(STATUS "ONNX Runtime found via paths: ${ONNXRUNTIME_LIBRARY}")
        set(HAS_ONNX TRUE)
    else()
        set(HAS_ONNX FALSE)
    endif()
endif()

# Define core logic library for testability
add_library(agent_core STATIC 
    sensors.cpp 
    actuators.cpp 
    controller.cpp 
    governor.cpp
    inference.cpp
    gpu_telemetry.cpp
)
target_link_libraries(agent_core PUBLIC shared)
target_link_libraries(agent_core PRIVATE User32.lib PowrProf.lib Pdh.lib bcrypt.lib)

if(NOT HAS_ONNX)
    message(WARNING "ONNX Runtime not found. AI Inference will be disabled in the build.")
    target_compile_definitions(agent_core PUBLIC NANOLOOP_DISABLE_AI)
else()
    if(onnxruntime_FOUND)
        target_link_libraries(agent_core PUBLIC onnxruntime)
    else()
        target_include_directories(agent_core PUBLIC ${ONNXRUNTIME_INCLUDE_DIR})
        target_link_libraries(agent_core PUBLIC ${ONNXRUNTIME_LIBRARY})
    endif()
endif()

# Main Agent Executable
add_executable(agent 
    main.cpp
    agent.manifest
)
target_link_libraries(agent PRIVATE agent_core)
