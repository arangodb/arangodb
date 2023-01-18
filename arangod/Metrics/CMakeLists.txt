add_library(arango_metrics STATIC
  Builder.cpp
  Counter.cpp
  Metric.cpp
  MetricsFeature.cpp
  ClusterMetricsFeature.cpp
  ${PROJECT_SOURCE_DIR}/arangod/RestHandler/RestMetricsHandler.cpp)

target_link_libraries(arango_metrics
  arango
  arango::validation
  arango_replication2
  immer
  rocksdb_interface)

target_include_directories(arango_metrics PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")
