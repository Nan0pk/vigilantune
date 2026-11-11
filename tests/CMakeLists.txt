include(FetchContent)
# Pinned to the v1.17.0 release by commit, so the build cannot change under us.
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG 52eb8108c5bdec04579160ae17225d66034bd723 # v1.17.0
)
# For Windows: prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

find_program(VIGILANTUNE_POWERSHELL NAMES pwsh powershell REQUIRED)
add_test(NAME UninstallSafety
  COMMAND "${VIGILANTUNE_POWERSHELL}" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/test_uninstall.ps1"
)

if(WIN32)
  add_executable(uninstall_fixture_agent test_uninstall_fixture.cpp)
  add_executable(uninstall_fixture_watchdog test_uninstall_fixture.cpp)
  set_target_properties(uninstall_fixture_agent PROPERTIES
    OUTPUT_NAME "VigilanTune.Agent" RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/uninstall-fixture")
  set_target_properties(uninstall_fixture_watchdog PROPERTIES
    OUTPUT_NAME "VigilanTune.Watchdog" RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/uninstall-fixture")
  add_test(NAME UninstallRealProcesses
    COMMAND "${VIGILANTUNE_POWERSHELL}" -NoProfile -NonInteractive -ExecutionPolicy Bypass
      -File "${CMAKE_CURRENT_SOURCE_DIR}/test_uninstall_live.ps1"
      -AgentFixture "$<TARGET_FILE:uninstall_fixture_agent>"
      -WatchdogFixture "$<TARGET_FILE:uninstall_fixture_watchdog>")
  set_tests_properties(UninstallRealProcesses PROPERTIES TIMEOUT 30)
endif()

# 1. Unit tests. Safe on any machine; these are what `ctest` runs.
add_executable(vigilantune_tests
  main.cpp
  test_database.cpp
  test_config.cpp
  test_controller.cpp
  test_heartbeat.cpp
  test_inference.cpp
  test_sensors.cpp
)
target_link_libraries(vigilantune_tests PRIVATE GTest::gtest_main agent_core)
gtest_discover_tests(vigilantune_tests)

# 2. Host tests. These create and activate a real Windows power plan, so they
# are built (to keep them compiling) but never registered with ctest. They move
# back into the unit tests once they run on a fake power backend (plan T1.1).
add_executable(vigilantune_host_tests
  main.cpp
  test_actuator.cpp
  test_pipeline.cpp
)
target_link_libraries(vigilantune_host_tests PRIVATE GTest::gtest_main agent_core)

# 3. Timing benchmarks. Results depend on the machine and build type, so they
# are registered with ctest only on request, under the label "benchmark":
#   cmake -B build -DVIGILANTUNE_BENCHMARK_TESTS=ON
#   ctest --test-dir build -C Release -L benchmark
option(VIGILANTUNE_BENCHMARK_TESTS "Register the timing benchmarks with ctest" OFF)
add_executable(vigilantune_benchmarks
  main.cpp
  test_benchmark.cpp
)
target_link_libraries(vigilantune_benchmarks PRIVATE GTest::gtest_main agent_core)
if(VIGILANTUNE_BENCHMARK_TESTS)
  gtest_discover_tests(vigilantune_benchmarks PROPERTIES LABELS benchmark)
endif()
