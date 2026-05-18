add_executable(watchdog 
    main.cpp
    watchdog.manifest
)

# Fix for Significant #8: Link shared and Win32 libs
target_link_libraries(watchdog PRIVATE shared PowrProf.lib)
