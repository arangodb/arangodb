add_library(arango_replication STATIC
  DatabaseInitialSyncer.cpp
  DatabaseTailingSyncer.cpp
  InitialSyncer.cpp
  ReplicationSyncConfiguration.cpp
  ReplicationClients.cpp
  ReplicationFeature.cpp
  ReplicationMetricsFeature.cpp
  ReplicationOptionsProvider.cpp
  Syncer.cpp
  SyncerId.cpp
  TailingSyncer.cpp
  common-defines.cpp
  utilities.cpp)

target_link_libraries(arango_replication
  arango_storage_engine
  arango_utils
  arango_vocbase)

target_include_directories(arango_replication PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")
