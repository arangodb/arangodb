add_executable(sepp EXCLUDE_FROM_ALL
  main.cpp
  Execution.cpp
  ExecutionThread.cpp
  Runner.cpp
  RocksDBOptions.cpp
  Server.cpp
  ValueGenerators/RandomStringGenerator.cpp
  Workloads/GetByPrimaryKey.cpp
  Workloads/InsertDocuments.cpp
  Workloads/IterateDocuments.cpp)

target_include_directories(sepp PRIVATE
  .
  ${PROJECT_SOURCE_DIR}/arangod)

target_link_libraries(sepp
  arango_rocksdb
  arangoserver
  rocksdb
  snappy
  boost_boost)

