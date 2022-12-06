add_library(arango_cluster_engine STATIC
  ClusterCollection.cpp
  ClusterEngine.cpp
  ClusterIndex.cpp
  ClusterIndexFactory.cpp
  ClusterRestCollectionHandler.cpp
  ClusterRestHandlers.cpp
  ClusterRestReplicationHandler.cpp
  ClusterRestWalHandler.cpp
  ClusterSelectivityEstimates.cpp
  ClusterTransactionCollection.cpp
  ClusterTransactionState.cpp
  ClusterV8Functions.cpp)

target_link_libraries(arango_cluster_engine
  arango_health
  arango_indexes
  boost_boost
  arango_v8
  arango_v8_no_deadline)

target_include_directories(arango_cluster_engine PUBLIC
  ${IRESEARCH_INCLUDE})
target_include_directories(arango_cluster_engine PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")

target_compile_options(arango_cluster_engine PRIVATE
	${CMAKE_DEPRECATE_OPTIONS})
