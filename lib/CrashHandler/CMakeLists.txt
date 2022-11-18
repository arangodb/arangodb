add_library(arango_crashhandler STATIC
  CrashHandler.cpp)
target_include_directories(arango_crashhandler
  PUBLIC ${PROJECT_SOURCE_DIR}/lib)

add_library(arango_crashhandler_light STATIC
  CrashHandlerLight.cpp)
target_include_directories(arango_crashhandler_light
  PUBLIC ${PROJECT_SOURCE_DIR}/lib)
