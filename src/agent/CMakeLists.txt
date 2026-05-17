# Find ONNX Runtime
find_path(ONNXRUNTIME_INCLUDE_DIR onnxruntime_cxx_api.h PATH_SUFFIXES onnxruntime)
find_library(ONNXRUNTIME_LIBRARY onnxruntime PATH_SUFFIXES lib)

# Define core logic library for testability
add_library(agent_core STATIC sensors.cpp actuators.cpp controller.cpp)
target_link_libraries(agent_core PUBLIC shared)
target_link_libraries(agent_core PRIVATE User32.lib PowrProf.lib Pdh.lib bcrypt.lib)

if(NOT ONNXRUNTIME_INCLUDE_DIR OR NOT ONNXRUNTIME_LIBRARY)
    message(WARNING "ONNX Runtime not found. AI Inference will be disabled in the build.")
    target_compile_definitions(agent_core PUBLIC WSPA_DISABLE_AI)
else()
    message(STATUS "ONNX Runtime found at: ${ONNXRUNTIME_LIBRARY}")
    target_sources(agent_core PRIVATE inference.cpp)
    target_include_directories(agent_core PUBLIC ${ONNXRUNTIME_INCLUDE_DIR})
    target_link_libraries(agent_core PUBLIC ${ONNXRUNTIME_LIBRARY})
endif()

# Main Agent Executable
add_executable(agent main.cpp)
target_link_libraries(agent PRIVATE agent_core)
