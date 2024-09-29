cmake_minimum_required(VERSION 3.10)

# project details
project(ScalableMPC VERSION 1.0)
set(TEST_EXEC memcheck)
set(OFFLINE_EXEC offline)
set(ONLINE_EXEC online)
set(LIBRARY_NAME scalable_mpc_lib)

# packages
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O3 -std=c++14 -maes -mavx -mpclmul -mbmi2 -msse2 -no-pie -Wno-ignored-attributes")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
include(Boost)
include(Cryptopp)
include(CTest)
include(Doctest)
include(Documentation)
include(Warnings)
include(Curses)
include(OpenSSL)
include(relic)

# libscapi
include_directories(/usr/include/openssl
  usr/local/include
  $ENV{HOME}
  $ENV{HOME}/libscapi/install/include
  $ENV{HOME}/libscapi/lib/OTExtensionBristol
  $ENV{HOME}/libscapi/install/include/libOTe/cryptoTools/
  $ENV{HOME}/libscapi/install/include/libOTe)
link_directories(/usr/lib/ssl $ENV{HOME}/libscapi/install/lib)

# add libraries
set(SOURCES
  src/pkg/enc.cxx
  src/pkg/eqtest.cxx
  src/pkg/garbling.cxx
  src/pkg/lpn.cxx
  src/pkg/pcg.cxx
  src/pkg/pprf.cxx
  src/pkg/rot.cxx
  src/ahe/ahe.cxx
  src/ahe/curve.cxx
  src/util/bitstring.cxx
  src/util/circuit.cxx
  src/util/concurrency.cxx
  src/util/random.cxx
  src/util/transpose.cxx
)

add_library(${LIBRARY_NAME} ${SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(${LIBRARY_NAME} PUBLIC doctest)
target_link_libraries(${LIBRARY_NAME} PRIVATE cryptopp)
target_link_libraries(${LIBRARY_NAME} PRIVATE cryptopp-shared)
target_link_libraries(${LIBRARY_NAME} PRIVATE sqlite3)
target_link_libraries(${LIBRARY_NAME} PRIVATE relic)
target_link_libraries(${LIBRARY_NAME} PRIVATE ${Boost_LIBRARIES})
target_link_libraries(${LIBRARY_NAME} PRIVATE ${CURSES_LIBRARIES})
target_link_libraries(${LIBRARY_NAME} PRIVATE ${OPENSSL_LIBRARIES})
target_link_libraries(${LIBRARY_NAME} PRIVATE
  $ENV{HOME}/libscapi/libscapi.a
  $ENV{HOME}/libscapi/install/lib/libOTExtensionBristol.a
  $ENV{HOME}/libscapi/install/lib/libOTe.a
  $ENV{HOME}/libscapi/install/lib/libsimpleot.a
  $ENV{HOME}/libscapi/install/lib/libcryptoTools.a
  gmp boost_system boost_thread boost_serialization pthread dl crypto ssl z)

# add executables
add_executable(${OFFLINE_EXEC} src/cmd/offline.cxx)
target_link_libraries(${OFFLINE_EXEC} PRIVATE ${LIBRARY_NAME})
target_link_libraries(${OFFLINE_EXEC} PRIVATE
  $ENV{HOME}/libscapi/libscapi.a
  $ENV{HOME}/libscapi/install/lib/libOTExtensionBristol.a
  $ENV{HOME}/libscapi/install/lib/libOTe.a
  $ENV{HOME}/libscapi/install/lib/libsimpleot.a
  $ENV{HOME}/libscapi/install/lib/libcryptoTools.a
  gmp boost_system boost_thread boost_serialization pthread dl crypto ssl z)

# properties
set_target_properties(
  ${LIBRARY_NAME}
  ${OFFLINE_EXEC}
    PROPERTIES
      CXX_STANDARD 14
      CXX_STANDARD_REQUIRED YES
      CXX_EXTENSIONS YES
)

add_executable(${TEST_EXEC} src/cmd/main.cxx)
target_link_libraries(${TEST_EXEC} PRIVATE ${LIBRARY_NAME})
target_link_libraries(${TEST_EXEC} PRIVATE
  $ENV{HOME}/libscapi/libscapi.a
  $ENV{HOME}/libscapi/install/lib/libOTExtensionBristol.a
  $ENV{HOME}/libscapi/install/lib/libOTe.a
  $ENV{HOME}/libscapi/install/lib/libsimpleot.a
  $ENV{HOME}/libscapi/install/lib/libcryptoTools.a
  gmp boost_system boost_thread boost_serialization pthread dl crypto ssl z)

# properties
set_target_properties(
  ${LIBRARY_NAME}
  ${TEST_EXEC}
    PROPERTIES
      CXX_STANDARD 14
      CXX_STANDARD_REQUIRED YES
      CXX_EXTENSIONS YES
)

# add tests
add_subdirectory(test)
add_custom_target(check ./test.sh)

# add benchmarks
add_subdirectory(benchmarks)
