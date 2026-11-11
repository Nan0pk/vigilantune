add_executable(watchdog 
    main.cpp
    watchdog.manifest
)
set_target_properties(watchdog PROPERTIES OUTPUT_NAME "VigilanTune.Watchdog")

# Fix for Significant #8: Link shared and Win32 libs
target_link_libraries(watchdog PRIVATE shared PowrProf.lib)

# watchdog.manifest sets the UAC level. Stop the linker adding its own
# "asInvoker" entry, which clashes with it and fails the link (mt.exe error
# c1010001).
if(MSVC)
    target_link_options(watchdog PRIVATE "/MANIFESTUAC:NO")
endif()
