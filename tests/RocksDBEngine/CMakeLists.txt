# FakeScheduler.cpp is intentionally NOT part of this object library: the
# combined arangodbtests binary already compiles it into arango_tests_basics, so
# including it here too would duplicate the symbols. The standalone executable
# below compiles it directly instead.
add_library(arango_tests_rocksdb_engine OBJECT
  RocksDBEngineTest.cpp
  DatabaseTest.cpp
  CollectionTest.cpp
  DocumentTest.cpp)

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
  velocypack_utils
  gtest)

add_test(NAME arangodbtests_rocksdb
  COMMAND arangodbtests_rocksdb)
