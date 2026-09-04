add_library(arango_system_monitor_async_registry STATIC
  Feature.cpp
  Metrics.cpp
  OptionsProvider.cpp
  RestHandler.cpp)

target_include_directories(arango_system_monitor_async_registry PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")

target_link_libraries(arango_system_monitor_async_registry
  PUBLIC
    arango_forest
    arango_basic_utils velocypack
    arango::validation rocksdb_interface immer arango_actor
    arango_iresearch)