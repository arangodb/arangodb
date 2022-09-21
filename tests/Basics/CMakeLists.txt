
add_executable(arangodbtests_basics
  ../../cmake/activeCodePage.manifest
  ${ARANGODB_TESTS_COMMON_SOURCES}
  ApplicationServerTest.cpp
  AttributeNameParserTest.cpp
  CMakeLists.txt
  ConversionsTest.cpp
  CpuUsageSnapshotTest.cpp
  CsvTest.cpp
  datetime.cpp
  EncodingUtilsTest.cpp
  EndpointTest.cpp
  FilesTest.cpp
  FixedSizeAllocatorTest.cpp
  FpConvTest.cpp
  GuardedTest.cpp
  HashesTest.cpp
  icu-helper.cpp
  icu-helper.h
  InspectionTest.cpp
  LoggerTest.cpp
  MemoryUsageTest.cpp
  NumberUtilsTest.cpp
  OverloadTest.cpp
  PriorityQueueTest.cpp
  RandomTest.cpp
  ReadWriteLockTest.cpp
  RecursiveLockerTest.cpp
  SpinLockTest.cpp
  StringBufferInternalTest.cpp
  StringBufferTest.cpp
  StringTest.cpp
  StringUtf8NormalizeTest.cpp
  StringUtf8Test.cpp
  StringUtilsTest.cpp
  UnshackledMutexTest.cpp
  VelocyPackHelperTest.cpp
 ../main.cpp)

target_link_libraries(arangodbtests_basics
  arango_agency
  arango_cluster_engine
  arango_rocksdb
  arango_v8server
  arangoserver
  arango_restore
  fuerte
  rocksdb
  snappy
  gtest
  boost_boost
  fmt
)

target_include_directories(arangodbtests_basics PRIVATE
  ${PROJECT_SOURCE_DIR}/arangod
  ${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/Mocks/
  ../
)

target_include_directories(arangodbtests_basics SYSTEM PRIVATE
  ${V8_INTERNAL_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}/3rdParty/fakeit-gtest
  ${CMAKE_SOURCE_DIR}/${GOOGLETEST_ROOT}/include
  ${CMAKE_SOURCE_DIR}/${GOOGLEMOCK_ROOT}/include
)

