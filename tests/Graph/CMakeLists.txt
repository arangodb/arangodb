add_library(arango_tests_graph OBJECT
  ClusterTraverserCacheTest.cpp
  TraverserCacheTest.cpp
  ConstantWeightShortestPathFinder.cpp
  GenericGraphProviderTest.cpp
  GraphMockProviderInstances.cpp
  GraphTestTools.cpp
  KPathFinderTest.cpp
  AllShortestPathsFinderTest.cpp
  DFSFinderTest.cpp
  KShortestPathsFinder.cpp
  PathStoreTest.cpp
  PathValidatorTest.cpp
  FifoQueueTest.cpp
  LifoQueueTest.cpp
  WeightedQueueTest.cpp
  SingleServerProviderTest.cpp
  ${ADDITIONAL_GRAPH_TEST_SOURCES})

target_link_libraries(arango_tests_graph
    arango
    arango_replication2
    arango_tests_basics
    velocypack
    fmt)

add_executable(arangodbtests_graph
  EXCLUDE_FROM_ALL)

target_link_libraries(arangodbtests_graph
    gtest
    arango_tests_basics
    arango_tests_graph
    arango_agency
    arango_cluster_engine
    arango_rocksdb
    arango_v8server
    arangoserver
    boost_boost
    fmt)
