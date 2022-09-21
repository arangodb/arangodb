
add_executable(arangodbtests_graph
  ../../cmake/activeCodePage.manifest
  ${ARANGODB_TESTS_COMMON_SOURCES}
  AllShortestPathsFinderTest.cpp
  ClusterTraverserCacheTest.cpp
  ConstantWeightShortestPathFinder.cpp
  DFSFinderTest.cpp
  FifoQueueTest.cpp
  GenericGraphProviderTest.cpp
  GraphMockProviderInstances.cpp
  GraphTestTools.cpp
  GraphTestTools.h
  KPathFinderTest.cpp
  KShortestPathsFinder.cpp
  LifoQueueTest.cpp
  PathStoreTest.cpp
  PathValidatorTest.cpp
  SingleServerProviderTest.cpp
  TraverserCacheTest.cpp
  WeightedQueueTest.cpp
  ../main.cpp)

target_link_libraries(arangodbtests_graph
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

target_include_directories(arangodbtests_graph PRIVATE
  ${PROJECT_SOURCE_DIR}/arangod
  ${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/Mocks/
  ../
)

target_include_directories(arangodbtests_graph SYSTEM PRIVATE
  ${V8_INTERNAL_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}/3rdParty/fakeit-gtest
  ${CMAKE_SOURCE_DIR}/${GOOGLETEST_ROOT}/include
  ${CMAKE_SOURCE_DIR}/${GOOGLEMOCK_ROOT}/include
)

