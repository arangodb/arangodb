option(BUILD_SEPP "Build SEPP (Storage Engine Performance Prediction)" OFF)

if (BUILD_SEPP)
  add_executable(sepp EXCLUDE_FROM_ALL
    main.cpp
    CacheOptions.cpp
    Execution.cpp
    ExecutionThread.cpp
    Runner.cpp
    RocksDBOptions.cpp
    Server.cpp
    ValueGenerators/RandomStringGenerator.cpp
    Workloads/EdgeCache.cpp
    Workloads/GetByPrimaryKey.cpp
    Workloads/InsertDocuments.cpp
    Workloads/IterateDocuments.cpp
    Workloads/WriteWriteConflict.cpp)

  target_include_directories(sepp PRIVATE
    .
    ${PROJECT_SOURCE_DIR}/arangod)

  target_link_libraries(sepp
    arango_rocksdb
    arangoserver
    rocksdb
    snappy
    boost_boost)
endif()
