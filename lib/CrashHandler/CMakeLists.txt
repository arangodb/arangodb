add_library(arango_crashhandler STATIC
  CrashHandler.cpp)
target_include_directories(arango_crashhandler
  PUBLIC
  ${PROJECT_SOURCE_DIR}/lib
  ${PROJECT_BINARY_DIR}/lib)
target_link_libraries(arango_crashhandler
  boost_system
  velocypack)

# A library that contains a crash handler that has
# no further dependencies
add_library(arango_crashhandler_light STATIC
  CrashHandlerLight.cpp)
target_include_directories(arango_crashhandler_light
  PUBLIC ${PROJECT_SOURCE_DIR}/lib)
