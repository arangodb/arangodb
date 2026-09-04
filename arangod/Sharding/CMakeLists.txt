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
    arango_iresearch)

if(USE_ENTERPRISE)
  target_link_libraries(arango_sharding
    PUBLIC
      arango_enterprise_sharding)
endif()

target_include_directories(arango_sharding PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")