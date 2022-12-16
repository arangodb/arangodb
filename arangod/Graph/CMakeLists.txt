add_library(arango_graph STATIC
  BaseOptions.cpp
  ClusterGraphDatalake.cpp
  ClusterTraverserCache.cpp
  Graph.cpp
  GraphManager.cpp
  GraphOperations.cpp
  KShortestPathsFinder.cpp
  ShortestPathFinder.cpp
  ShortestPathOptions.cpp
  ShortestPathResult.cpp
  PathType.cpp
  SingleServerEdgeCursor.cpp
  TraverserCache.cpp # maybe remove
  TraverserCacheFactory.cpp
  TraverserDocumentCache.cpp
  TraverserOptions.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Cluster/CallbackGuard.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Cluster/ClusterEdgeCursor.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Cluster/ServerDefaults.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Cluster/TraverserEngine.cpp
  ${PROJECT_SOURCE_DIR}/arangod/InternalRestHandler/InternalRestTraverserHandler.cpp
  ${PROJECT_SOURCE_DIR}/arangod/RestHandler/RestGraphHandler.cpp)

target_link_libraries(arango_graph
  arango
  arango_aql
  arango_cache
  arango_vocbase
  boost_boost)

target_include_directories(arango_graph PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")

add_subdirectory(Cache)
add_subdirectory(Cursors)
add_subdirectory(Enumerators)
add_subdirectory(Helpers)
add_subdirectory(Options)
add_subdirectory(PathManagement)
add_subdirectory(Providers)
add_subdirectory(Queues)
add_subdirectory(Steps)
add_subdirectory(Types)
