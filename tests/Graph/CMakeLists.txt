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
  SingleServerProviderTest.cpp)

target_link_libraries(arango_tests_graph
  PRIVATE
    arango
    arango_tests_basics
    velocypack
    fmt)

add_executable(arangodbtests_graph
  EXCLUDE_FROM_ALL)

target_link_libraries(arangodbtests_graph
    arango_tests_graph
    arango_tests_basics
    arango_agency
    arango_cluster_engine
    arango_rocksdb
    arango_v8server
    arangoserver
    boost_boost)

add_test(NAME graph
         COMMAND arangodbtests_graph)
