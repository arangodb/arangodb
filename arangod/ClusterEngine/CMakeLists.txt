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
  ClusterTransactionState.cpp)

if (USE_V8) 
  target_sources(arango_cluster_engine PRIVATE
    ClusterV8Functions.cpp)
endif()

target_link_libraries(arango_cluster_engine
  arango_health
  arango_indexes
  boost_boost)

if (USE_V8)
  target_link_libraries(arango_cluster_engine arango_v8 arango_v8_no_deadline)
endif()

target_include_directories(arango_cluster_engine PUBLIC
  ${IRESEARCH_INCLUDE})
target_include_directories(arango_cluster_engine PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")
