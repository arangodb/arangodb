add_executable(arangodbtests_rocksdb
  RocksDBEngineMinimalTest.cpp)

target_link_libraries(arangodbtests_rocksdb
  arango_rocksdb
  arango_metrics_base
  gtest
  gtest_main)

target_include_directories(arangodbtests_rocksdb PRIVATE
  ${PROJECT_SOURCE_DIR}/arangod
  ${PROJECT_SOURCE_DIR}/lib
  ${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR})

add_test(NAME arangodbtests_rocksdb
  COMMAND arangodbtests_rocksdb)
