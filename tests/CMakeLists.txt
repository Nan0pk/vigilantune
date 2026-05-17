include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/heads/main.zip
)
# For Windows: prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(wspa_tests
  main.cpp
  test_database.cpp
  test_controller.cpp
  test_actuator.cpp
)

target_link_libraries(wspa_tests
  PRIVATE
    GTest::gtest_main
    agent_core
)

include(GoogleTest)
gtest_discover_tests(wspa_tests)
