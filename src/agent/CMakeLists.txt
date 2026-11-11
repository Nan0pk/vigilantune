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
)
target_link_libraries(agent_core PUBLIC shared)
target_link_libraries(agent_core PRIVATE User32.lib PowrProf.lib Pdh.lib bcrypt.lib)

if(NOT HAS_ONNX)
    message(WARNING "ONNX Runtime not found. AI Inference will be disabled in the build.")
    target_compile_definitions(agent_core PUBLIC VIGILANTUNE_DISABLE_AI)
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
set_target_properties(agent PROPERTIES OUTPUT_NAME "VigilanTune.Agent")
target_link_libraries(agent PRIVATE agent_core)

# agent.manifest sets the UAC level. Stop the linker adding its own "asInvoker"
# entry, which clashes with it and fails the link (mt.exe error c1010001).
if(MSVC)
    target_link_options(agent PRIVATE "/MANIFESTUAC:NO")
endif()
