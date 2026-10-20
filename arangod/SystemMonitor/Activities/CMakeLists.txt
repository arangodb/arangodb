add_library(arango_system_monitor_activities STATIC
  Feature.cpp
  Metrics.cpp
  OptionsProvider.cpp
  RestHandler.cpp)

target_include_directories(arango_system_monitor_activities PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")

target_link_libraries(arango_system_monitor_activities
  PUBLIC
    arango_basic_utils velocypack
    arango::validation rocksdb_interface immer arango_actor
    arango_iresearch arango_v8)