cmake_minimum_required(VERSION 3.14)

# list of benchmark files
set(BENCHMARK_FILES
  main.cxx
  aes.cxx
)
set(BENCHMARK_EXEC foobar)

# --------------------------------------------------------------------------------
#                         Make Tests (no change needed).
# --------------------------------------------------------------------------------

# set up GoogleTest
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)

# for windows: prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
enable_testing()

add_executable(${BENCHMARK_EXEC} ${BENCHMARK_FILES})
set_target_properties(${BENCHMARK_EXEC} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set_target_properties(${BENCHMARK_EXEC} PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS YES
)
target_link_libraries(${BENCHMARK_EXEC}
  GTest::gtest_main
  ${LIBRARY_NAME}
)
include(GoogleTest)
gtest_discover_tests(${BENCHMARK_EXEC})
