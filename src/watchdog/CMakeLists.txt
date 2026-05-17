add_executable(watchdog main.cpp)

# Add Win32 libs
target_link_libraries(watchdog PRIVATE PowrProf.lib)
