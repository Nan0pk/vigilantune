add_executable(agent main.cpp sensors.cpp actuators.cpp controller.cpp inference.cpp)
target_link_libraries(agent PRIVATE shared)

# Add Win32 libs
target_link_libraries(agent PRIVATE User32.lib PowrProf.lib Pdh.lib)

# ONNX Runtime - In production, use find_package or provide direct paths
# target_link_libraries(agent PRIVATE onnxruntime.lib)
# target_include_directories(agent PRIVATE ${ONNXRUNTIME_INCLUDE_DIR})
