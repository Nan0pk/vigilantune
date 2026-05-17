add_executable(agent main.cpp sensors.cpp)
target_link_libraries(agent PRIVATE shared)

# Add Win32 libs
target_link_libraries(agent PRIVATE User32.lib PowrProf.lib)
