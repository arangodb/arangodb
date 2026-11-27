target_sources(arango_tests_graph
  PRIVATE
    SingleServerTraversalEnumeratorTest.cpp)

add_executable(arangodbtests_simple_traversal EXCLUDE_FROM_ALL
  GraphViewBFSTest.cpp
  InMemoryGraphTest.cpp
  InteriorTest.cpp)

target_link_libraries(arangodbtests_simple_traversal
  arango_lightweight
  gtest_main)
