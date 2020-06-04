# -*- mode: CMAKE; -*-



# add sources for cluster engine
set(CLUSTER_ENGINE_SOURCES
  ClusterEngine/ClusterCollection.cpp
  ClusterEngine/ClusterEngine.cpp
  ClusterEngine/ClusterIndex.cpp
  ClusterEngine/ClusterIndexFactory.cpp
  ClusterEngine/ClusterRestCollectionHandler.cpp
  ClusterEngine/ClusterRestExportHandler.cpp
  ClusterEngine/ClusterRestHandlers.cpp
  ClusterEngine/ClusterRestReplicationHandler.cpp
  ClusterEngine/ClusterRestWalHandler.cpp
  ClusterEngine/ClusterSelectivityEstimates.cpp
  ClusterEngine/ClusterTransactionCollection.cpp
  ClusterEngine/ClusterTransactionState.cpp
  ClusterEngine/ClusterV8Functions.cpp
  ClusterEngine/RocksDBMethods.cpp
)
set(CLUSTER_ENGINE_SOURCES ${CLUSTER_ENGINE_SOURCES} PARENT_SCOPE)
