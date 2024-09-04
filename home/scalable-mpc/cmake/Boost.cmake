SET(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.71 REQUIRED COMPONENTS thread system filesystem log log_setup program_options)
include_directories( ${Boost_INCLUDE_DIR} )
