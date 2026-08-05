add_library(arango_system_monitor_activities STATIC
  Feature.cpp
  Metrics.cpp
  OptionsProvider.cpp
  RestHandler.cpp)

target_include_directories(arango_system_monitor_activities PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")