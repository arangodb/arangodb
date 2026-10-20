add_library(arango_cluster_methods STATIC
  ClusterAdminOperations.cpp
  ClusterHotBackup.cpp
  ClusterMethods.cpp
  ClusterCollectionMethods.cpp
  ClusterIndexMethods.cpp
  ClusterTtlMethods.cpp)

target_link_libraries(arango_cluster_methods
  arango
  arango_network
  zlib_interface)

if(USE_ENTERPRISE)
  target_link_libraries(arango_cluster_methods
    arango_enterprise_audit)
endif()

target_include_directories(arango_cluster_methods PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")

target_compile_options(arango_cluster_methods PRIVATE
	${CMAKE_DEPRECATE_OPTIONS})

add_subdirectory(Utils)

add_library(arango_cluster STATIC
  Action.cpp
  ActionBase.cpp
  ActionDescription.cpp
  AgencyCache.cpp
  AgencyCallback.cpp
  AgencyCallbackRegistry.cpp
  AutoRebalance.cpp
  ClusterCollectionCreationInfo.cpp
  ClusterFeature.cpp
  ClusterOptionsProvider.cpp
  ClusterHelpers.cpp
  ClusterInfo.cpp
  ClusterTrxMethods.cpp
  ClusterTypes.cpp
  ClusterUpgradeFeature.cpp
  ClusterUpgradeOptionsProvider.cpp
  CollectionInfoCurrent.cpp
  CreateCollection.cpp
  CreateDatabase.cpp
  DBServerAgencySync.cpp
  DropCollection.cpp
  DropDatabase.cpp
  DropIndex.cpp
  EnsureIndex.cpp
  FollowerInfo.cpp
  HeartbeatThread.cpp
  Maintenance.cpp
  MaintenanceFeature.cpp
  MaintenanceOptionsProvider.cpp
  MaintenanceRestHandler.cpp
  MaintenanceWorker.cpp
  RebootTracker.cpp
  ReplicationTimeoutFeature.cpp
  ReplicationTimeoutOptionsProvider.cpp
  ResignShardLeadership.cpp
  RestAgencyCallbacksHandler.cpp
  RestClusterHandler.cpp
  ServerDefaults.cpp
  ServerState.cpp
  SynchronizeShard.cpp
  TakeoverShardLeadership.cpp
  UpdateCollection.cpp
  UpdateReplicatedLogAction.cpp)

target_link_libraries(arango_cluster
  PUBLIC
    arango_auth
    arango_basic_utils velocypack
    arango::validation rocksdb_interface immer arango_actor
    arango_iresearch arango_v8)

target_include_directories(arango_cluster PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")
