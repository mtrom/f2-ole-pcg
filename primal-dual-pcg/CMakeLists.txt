cmake_minimum_required(VERSION 3.10)

# project details
project(primal-dual-pcg VERSION 1.0)
set(FRONTEND_EXEC protocol)
set(LIBRARY_NAME primal_dual_pcg)

# packages
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O3 -std=c++20 -maes -mavx -mpclmul -mbmi2 -msse2 -no-pie -Wno-ignored-attributes")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
include(Boost)
include(OpenSSL)
include(libOTe)

include_directories(
  /usr/include/openssl
  usr/local/include
  $ENV{HOME}/libOTe/cryptoTools
  $ENV{HOME}
)
link_directories(/usr/lib/ssl)

# add libraries
set(SOURCES
  src/pkg/eqtest.cxx
  src/pkg/lpn.cxx
  src/pkg/pcg.cxx
  src/pkg/pprf.cxx
  src/pkg/rot.cxx
  src/ahe/ahe.cxx
  src/util/bitstring.cxx
  src/util/concurrency.cxx
  src/util/random.cxx
  src/util/transpose.cxx
)

add_library(${LIBRARY_NAME} ${SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(${LIBRARY_NAME} PRIVATE ${Boost_LIBRARIES})
target_link_libraries(${LIBRARY_NAME} PRIVATE ${OPENSSL_LIBRARIES})
target_link_libraries(${LIBRARY_NAME} PRIVATE oc::cryptoTools oc::libOTe)
target_link_libraries(${LIBRARY_NAME} PRIVATE
  gmp boost_system boost_thread boost_filesystem pthread dl crypto ssl
)

# add executables
add_executable(${FRONTEND_EXEC} src/cmd/main.cxx)
target_link_libraries(${FRONTEND_EXEC} PRIVATE ${LIBRARY_NAME})
target_link_libraries(${FRONTEND_EXEC} PRIVATE ${Boost_LIBRARIES})
target_link_libraries(${FRONTEND_EXEC} PRIVATE ${Boost_FILESYSTEM_LIBRARY})
target_link_libraries(${FRONTEND_EXEC} PRIVATE ${Boost_PROGRAM_OPTIONS_LIBRARY})
target_link_libraries(${FRONTEND_EXEC} PRIVATE ${Boost_SYSTEM_LIBRARY})
target_link_libraries(${FRONTEND_EXEC} PRIVATE
  gmp boost_system boost_thread pthread dl crypto ssl
)

# properties
set_target_properties(
  ${LIBRARY_NAME}
  ${FRONTEND_EXEC}
    PROPERTIES
      CXX_STANDARD 20
      CXX_STANDARD_REQUIRED YES
      CXX_EXTENSIONS YES
)

# add tests
add_subdirectory(test)
add_custom_target(check ./test.sh)
