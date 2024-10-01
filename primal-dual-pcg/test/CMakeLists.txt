cmake_minimum_required(VERSION 3.14)

set(TEST_FILES
  main.cxx
  test_ahe.cxx
  test_bitstring.cxx
  test_eqtest.cxx
  test_lpn.cxx
  test_pcg.cxx
  test_pprf.cxx
  test_random.cxx
  test_rot.cxx
)
set(TEST_MAIN unit_tests)

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

add_executable(${TEST_MAIN} ${TEST_FILES})
set_target_properties(${TEST_MAIN} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set_target_properties(${TEST_MAIN} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS YES
)
target_include_directories(${LIBRARY_NAME} PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(${TEST_MAIN}
  GTest::gtest_main
  ${LIBRARY_NAME}
)
include(GoogleTest)
gtest_discover_tests(${TEST_MAIN})
