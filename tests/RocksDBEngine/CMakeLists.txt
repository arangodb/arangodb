# FakeScheduler.cpp is intentionally NOT part of this object library: the
# combined arangodbtests binary already compiles it into arango_tests_basics, so
# including it here too would duplicate the symbols. The standalone executable
# below compiles it directly instead.
add_library(arango_tests_rocksdb_engine OBJECT
  StorageEngineFixture.cpp
  RocksDBEngineTest.cpp
  DatabaseTest.cpp
  CollectionTest.cpp
  DocumentTest.cpp
  IndexTest.cpp)

target_include_directories(arango_tests_rocksdb_engine PRIVATE
  ${PROJECT_SOURCE_DIR}/arangod
  ${PROJECT_SOURCE_DIR}/lib
  ${PROJECT_SOURCE_DIR}/tests
  ${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR})

target_link_libraries(arango_tests_rocksdb_engine
  PRIVATE
    arango_rocksdb
    arango_metrics_base
    arango_vocbase
    gtest
    gmock)

# Own main() (instead of gtest_main) so we can perform the process-wide startup
# initialisation the engine's transaction machinery expects (async/activities
# registries).
add_executable(arangodbtests_rocksdb
  main.cpp
  RestHandlersStub.cpp
  ${PROJECT_SOURCE_DIR}/tests/Mocks/FakeScheduler.cpp)

target_include_directories(arangodbtests_rocksdb PRIVATE
  ${PROJECT_SOURCE_DIR}/arangod
  ${PROJECT_SOURCE_DIR}/lib
  ${PROJECT_SOURCE_DIR}/tests
  ${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR})

target_link_libraries(arangodbtests_rocksdb
  arango_tests_rocksdb_engine
  arango_rocksdb
  arango_metrics_base
  arango_vocbase
  arango_scheduler  # FakeScheduler
  arango_rest_server  # ServerIdFeature
  arango_sharding # ShardingFeature
  arango_system_monitor_activities  # main.cpp sets up registries
  arango_system_monitor_async_registry  # main.cpp sets up registries
  arango_vector_index # mock provider
  velocypack_utils
  gtest
  gmock)

add_test(NAME arangodbtests_rocksdb
  COMMAND arangodbtests_rocksdb)
