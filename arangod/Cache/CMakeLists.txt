add_library(arango_cache STATIC
  BucketState.cpp
  Cache.cpp
  CacheManagerFeature.cpp
  CacheManagerFeatureThreads.cpp
  CacheOptionsFeature.cpp
  CachedValue.cpp
  Finding.cpp
  Manager.cpp
  ManagerTasks.cpp
  Metadata.cpp
  PlainBucket.cpp
  PlainCache.cpp
  Rebalancer.cpp
  Table.cpp
  Transaction.cpp
  TransactionManager.cpp
  TransactionalBucket.cpp
  TransactionalCache.cpp)

target_link_libraries(arango_cache
  arango
  arango::validation
  boost_system)

target_include_directories(arango_cache PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")
