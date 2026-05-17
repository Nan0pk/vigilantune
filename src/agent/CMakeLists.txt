# Find ONNX Runtime
# Set ONNXRUNTIME_ROOT_DIR to your ONNX Runtime installation path
find_path(ONNXRUNTIME_INCLUDE_DIR onnxruntime_cxx_api.h PATH_SUFFIXES onnxruntime)
find_library(ONNXRUNTIME_LIBRARY onnxruntime PATH_SUFFIXES lib)

if(NOT ONNXRUNTIME_INCLUDE_DIR OR NOT ONNXRUNTIME_LIBRARY)
    message(WARNING "ONNX Runtime not found. AI Inference will be disabled in the build. Please set ONNXRUNTIME_ROOT_DIR.")
    add_definitions(-DWSPA_DISABLE_AI)
    add_executable(agent main.cpp sensors.cpp actuators.cpp controller.cpp)
else()
    message(STATUS "ONNX Runtime found at: ${ONNXRUNTIME_LIBRARY}")
    add_executable(agent main.cpp sensors.cpp actuators.cpp controller.cpp inference.cpp)
    target_include_directories(agent PRIVATE ${ONNXRUNTIME_INCLUDE_DIR})
    target_link_libraries(agent PRIVATE ${ONNXRUNTIME_LIBRARY})
endif()

target_link_libraries(agent PRIVATE shared)

# Add Win32 libs
target_link_libraries(agent PRIVATE User32.lib PowrProf.lib Pdh.lib)
