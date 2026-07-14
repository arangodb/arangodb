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

add_executable(probe_statistics
  ${PROJECT_SOURCE_DIR}/tests/Probe/probe_main.cpp)

target_link_options(probe_statistics PRIVATE
  "-Wl,--whole-archive"
  "$<TARGET_FILE:arango_statistics>"
  "-Wl,--no-whole-archive")

target_link_libraries(probe_statistics PRIVATE arango_statistics)
