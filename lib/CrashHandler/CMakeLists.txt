add_library(arango_crashhandler STATIC
	CrashHandler.cpp)

target_include_directories(arango_crashhandler
  PUBLIC
  ${PROJECT_SOURCE_DIR}/lib
  ${PROJECT_BINARY_DIR}/lib)
target_link_libraries(arango_crashhandler
  # TODO: The dependency on arango can be removed
  #       once we can reliably access functions to
  #       convert memory units that are currently
  #       in ProgramOptions/Parameters.h and
  #       the function that accesses the environment
  #       through files.h
  arango
  boost_system
  velocypack)

# A library that contains a crash handler that has
# no further dependencies
add_library(arango_crashhandler_light STATIC
  CrashHandlerLight.cpp)
target_include_directories(arango_crashhandler_light
  PUBLIC ${PROJECT_SOURCE_DIR}/lib)
