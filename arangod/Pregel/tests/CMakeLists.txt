add_executable(arangodbtests_wcc
  ${PROJECT_SOURCE_DIR}/lib/Basics/VelocyPackStringLiteral.cpp
  GraphsSource.cpp
  WeaklyConnectedComponentsTest.cpp
  Graph.cpp
  WCCGraph.cpp
  DisjointSet.cpp)

add_executable(arangodbtests_scc
        ${PROJECT_SOURCE_DIR}/lib/Basics/VelocyPackStringLiteral.cpp
        GraphsSource.cpp
        StronglyConnectedComponentsTest.cpp
        Graph.cpp
        SCCGraph.cpp
        DisjointSet.cpp)

add_executable(arangodbtests_disjoint_set
        ${PROJECT_SOURCE_DIR}/lib/Basics/VelocyPackStringLiteral.cpp
        DisjointSetTest.cpp
        GraphsSource.cpp
        DisjointSet.cpp)

target_include_directories(arangodbtests_wcc PRIVATE
  ${PROJECT_SOURCE_DIR}/lib
  ${PROJECT_BINARY_DIR}/lib
  ${PROJECT_SOURCE_DIR}/arangod/Pregel)

target_include_directories(arangodbtests_scc PRIVATE
        ${PROJECT_SOURCE_DIR}/lib
        ${PROJECT_BINARY_DIR}/lib
        ${PROJECT_SOURCE_DIR}/arangod/Pregel)

target_include_directories(arangodbtests_disjoint_set PRIVATE
        ${PROJECT_SOURCE_DIR}/lib
        ${PROJECT_BINARY_DIR}/lib
        ${PROJECT_SOURCE_DIR}/arangod/Pregel)

target_link_libraries(arangodbtests_wcc velocypack fmt gtest)
target_link_libraries(arangodbtests_scc velocypack fmt gtest)
target_link_libraries(arangodbtests_disjoint_set velocypack fmt gtest)
