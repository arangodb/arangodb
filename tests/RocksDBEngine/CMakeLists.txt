add_executable(arango_rocksdb_minimal_test
  RocksDBEngineMinimalTest.cpp)

target_link_libraries(arango_rocksdb_minimal_test
  arango_rocksdb
  arangoserver
  gtest
  gtest_main)

target_include_directories(arango_rocksdb_minimal_test PRIVATE
  ${PROJECT_SOURCE_DIR}/arangod
  ${PROJECT_SOURCE_DIR}/lib
  ${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR})

add_test(NAME arango_rocksdb_minimal_test
  COMMAND arango_rocksdb_minimal_test)
