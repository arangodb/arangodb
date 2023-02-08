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
  PRIVATE
    arango_lightweight
    boost_system
    velocypack)

# TODO: With an update of libunwind to 1.6 or above
# we will get a CMakeLists for libunwind and be able to use
# cleaner include and linking.
if (USE_LIBUNWIND)
  target_link_libraries(arango_crashhandler
    PUBLIC
      ${LIBUNWIND_LIB})

  target_include_directories(arango_crashhandler
    PRIVATE
      "${LIBUNWIND_HOME}/include")

  add_dependencies(arango_crashhandler libunwind_build)
endif ()


# A library that contains a crash handler that has
# no further dependencies
add_library(arango_crashhandler_light STATIC
  CrashHandlerLight.cpp)
target_include_directories(arango_crashhandler_light
  PUBLIC ${PROJECT_SOURCE_DIR}/lib)
