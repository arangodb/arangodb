add_library(arango_tests_graph OBJECT
        ClusterTraverserCacheTest.cpp
        TraverserCacheTest.cpp
        GenericGraphProviderTest.cpp
        GraphMockProviderInstances.cpp
        GraphTestTools.cpp
        KPathFinderTest.cpp
        AllShortestPathsFinderTest.cpp
        DFSFinderTest.cpp
        PathStoreTest.cpp
        PathValidatorTest.cpp
        FifoQueueTest.cpp
        LifoQueueTest.cpp
        WeightedQueueTest.cpp
        KShortestPathsFinderTest.cpp
        WeightedShortestPathTest.cpp
        SingleServerProviderTest.cpp)

target_link_libraries(arango_tests_graph
        PRIVATE
        arango_lightweight
        arango_tests_basics
        velocypack
        fmt)

add_executable(arangodbtests_graph
        EXCLUDE_FROM_ALL)

target_link_libraries(arangodbtests_graph
        arango_tests_graph
        arango_tests_basics)

add_test(test_all_graphs arango_tests_graph)

add_test(NAME graph
        COMMAND arangodbtests_graph)
