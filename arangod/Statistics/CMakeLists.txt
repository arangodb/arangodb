add_library(arango_statistics STATIC
  ConnectionStatistics.cpp
  Descriptions.cpp
  RequestStatistics.cpp
  StatisticsFeature.cpp
  StatisticsWorker.cpp
  StatisticsOptionsProvider.cpp)

target_link_libraries(arango_statistics
  PUBLIC
    arango_basic_utils velocypack
    arango::validation rocksdb_interface immer arango_actor
    arango_iresearch arango_v8)

target_include_directories(arango_statistics PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")