add_library(arango_health STATIC
  HealthData.cpp)
target_link_libraries(arango_health arango)
target_include_directories(arango_health PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")

add_library(arango_storage_engine STATIC
  EngineSelectorFeature.cpp
  HotBackup.cpp
  PhysicalCollection.cpp
  TransactionCollection.cpp
  TransactionState.cpp
  StorageEngine.cpp
  VPackSortMigration.cpp)
target_link_libraries(arango_storage_engine
  arango_cluster_engine
  arango_cluster_methods
  arango_rocksdb
  arango::validation
  arango_replication2
  immer)
target_include_directories(arango_storage_engine PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")

# arango_storage_engine uses arango_rocksdb.
# In turn, arango_storage_engine_common provides something for both.
add_library(arango_storage_engine_common STATIC
  ReplicationIterator.cpp
  WalAccess.cpp)
target_link_libraries(arango_storage_engine_common
  arango::validation
  arango_replication2
  immer)
target_include_directories(arango_storage_engine_common PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")
