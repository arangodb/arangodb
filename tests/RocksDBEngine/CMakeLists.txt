add_library(arango_tests_rocksdb_engine OBJECT
  RocksDBEngineMinimalTest.cpp
  ${PROJECT_SOURCE_DIR}/tests/Mocks/FakeScheduler.cpp)

target_include_directories(arango_tests_rocksdb_engine PRIVATE
  ${PROJECT_SOURCE_DIR}/arangod
  ${PROJECT_SOURCE_DIR}/lib
  ${PROJECT_SOURCE_DIR}/tests
  ${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR})

target_link_libraries(arango_tests_rocksdb_engine
  PRIVATE
    arango_rocksdb_core
    arango_rocksdb_replication
    arango_rocksdb_dump
    arango_rocksdb_indexes
    arango_rocksdb_rest
    arango_rocksdb_aql
    arango_rocksdb_v8
    arango_restart_action
    arango_metrics_base
    gtest
    gmock)

add_executable(arangodbtests_rocksdb)

target_link_libraries(arangodbtests_rocksdb
  arango_tests_rocksdb_engine
  gtest_main)

add_test(NAME arangodbtests_rocksdb
  COMMAND arangodbtests_rocksdb)
