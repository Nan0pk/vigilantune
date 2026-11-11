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

# 1. Unit tests. Safe on any machine; these are what `ctest` runs.
add_executable(nanoloop_tests
  main.cpp
  test_database.cpp
  test_controller.cpp
  test_heartbeat.cpp
  test_inference.cpp
  test_sensors.cpp
)
target_link_libraries(nanoloop_tests PRIVATE GTest::gtest_main agent_core)
gtest_discover_tests(nanoloop_tests)

# 2. Host tests. These create and activate a real Windows power plan, so they
# are built (to keep them compiling) but never registered with ctest. They move
# back into the unit tests once they run on a fake power backend (plan T1.1).
add_executable(nanoloop_host_tests
  main.cpp
  test_actuator.cpp
  test_pipeline.cpp
)
target_link_libraries(nanoloop_host_tests PRIVATE GTest::gtest_main agent_core)

# 3. Timing benchmarks. Results depend on the machine and build type, so they
# are registered with ctest only on request, under the label "benchmark":
#   cmake -B build -DNANOLOOP_BENCHMARK_TESTS=ON
#   ctest --test-dir build -C Release -L benchmark
option(NANOLOOP_BENCHMARK_TESTS "Register the timing benchmarks with ctest" OFF)
add_executable(nanoloop_benchmarks
  main.cpp
  test_benchmark.cpp
)
target_link_libraries(nanoloop_benchmarks PRIVATE GTest::gtest_main agent_core)
if(NANOLOOP_BENCHMARK_TESTS)
  gtest_discover_tests(nanoloop_benchmarks PROPERTIES LABELS benchmark)
endif()
