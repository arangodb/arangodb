add_library(arango_sharding STATIC
  ShardDistributionReporter.cpp
  ShardingFeature.cpp
  ShardingInfo.cpp
  ShardingStrategy.cpp
  ShardingStrategyDefault.cpp)

target_link_libraries(arango_sharding
  PUBLIC
    arango_basic_utils velocypack
    arango::validation rocksdb_interface immer arango_actor
    arango_iresearch arango_v8)

target_include_directories(arango_sharding PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")

add_executable(probe_sharding
  ${PROJECT_SOURCE_DIR}/tests/Probe/probe_main.cpp)

target_link_options(probe_sharding PRIVATE
  "-Wl,--whole-archive"
  "$<TARGET_FILE:arango_sharding>"
  "-Wl,--no-whole-archive")

target_link_libraries(probe_sharding PRIVATE arango_sharding)
