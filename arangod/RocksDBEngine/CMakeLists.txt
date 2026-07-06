# ---- Core: engine primitives, transactions, collection ----
add_library(arango_rocksdb_core STATIC
  RocksDBBackgroundThread.cpp
  RocksDBCFStats.cpp
  RocksDBChecksumEnv.cpp
  RocksDBCollection.cpp
  RocksDBColumnFamilyManager.cpp
  RocksDBCommon.cpp
  RocksDBComparator.cpp
  RocksDBCuckooIndexEstimator.cpp
  RocksDBEngine.cpp
  RocksDBEngineOptionsProvider.cpp
  RocksDBFormat.cpp
  RocksDBIterators.cpp
  RocksDBKey.cpp
  RocksDBKeyBounds.cpp
  RocksDBLogValue.cpp
  RocksDBMetaCollection.cpp
  RocksDBMetadata.cpp
  RocksDBMethods.cpp
  RocksDBMethodsMemoryTracker.cpp
  RocksDBOptionFeature.cpp
  RocksDBOptionFeatureOptionsProvider.cpp
  RocksDBOptionsProvider.cpp
  RocksDBReadWriteMetrics.cpp
  RocksDBRecoveryManager.cpp
  RocksDBSavePoint.cpp
  RocksDBSettingsManager.cpp
  RocksDBSyncThread.cpp
  RocksDBTempStorage.cpp
  RocksDBTransactionCollection.cpp
  RocksDBTransactionMethods.cpp
  RocksDBTransactionState.cpp
  RocksDBTypes.cpp
  RocksDBUpgrade.cpp
  RocksDBValue.cpp
  ReplicatedRocksDBTransactionCollection.cpp
  ReplicatedRocksDBTransactionState.cpp
  SimpleRocksDBTransactionState.cpp)

target_link_libraries(arango_rocksdb_core
  PUBLIC
    rocksdb
    rocksdb_interface
    arango_metrics_base
    immer
    taocpp::json
    arango::validation
    arango_actor
  PRIVATE
    snappy
    arango_health)

# ---- Indexes ----
add_library(arango_rocksdb_indexes STATIC
  RocksDBBuilderIndex.cpp
  RocksDBEdgeIndex.cpp
  RocksDBFulltextIndex.cpp
  RocksDBGeoIndex.cpp
  RocksDBIndex.cpp
  RocksDBIndexCacheRefillFeature.cpp
  RocksDBIndexCacheRefillFeatureOptions.cpp
  RocksDBIndexCacheRefillOptionsProvider.cpp
  RocksDBIndexCacheRefillThread.cpp
  RocksDBIndexFactory.cpp
  RocksDBIndexingDisabler.cpp
  RocksDBMultiDimIndex.cpp
  RocksDBPrimaryIndex.cpp
  RocksDBTtlIndex.cpp
  RocksDBVectorIndex.cpp
  RocksDBVectorIndexBuilder.cpp
  RocksDBVectorIndexList.cpp
  RocksDBVPackIndex.cpp)

target_link_libraries(arango_rocksdb_indexes
  PUBLIC
    arango_rocksdb_core
    arango_indexes
    arango_cache
    arango_zkd
    faiss)

# ---- Replication / WAL ----
add_library(arango_rocksdb_replication STATIC
  RocksDBIncrementalSync.cpp
  RocksDBReplicationCommon.cpp
  RocksDBReplicationContext.cpp
  RocksDBReplicationContextGuard.cpp
  RocksDBReplicationIterator.cpp
  RocksDBReplicationManager.cpp
  RocksDBReplicationTailing.cpp
  RocksDBWalAccess.cpp)

target_link_libraries(arango_rocksdb_replication
  PUBLIC 
    arango_rocksdb_core
    arango_storage_engine_common)

# ---- Dump / sorted rows ----
add_library(arango_rocksdb_dump STATIC
  RocksDBDumpContext.cpp
  RocksDBDumpManager.cpp
  RocksDBSortedRowsStorageContext.cpp
  SortedRowsStorageBackendRocksDB.cpp)

target_link_libraries(arango_rocksdb_dump
  PUBLIC arango_rocksdb_core)

# ---- REST handlers (server only) ----
add_library(arango_rocksdb_rest STATIC
  RocksDBRestCollectionHandler.cpp
  RocksDBRestHandlers.cpp
  RocksDBRestReplicationHandler.cpp
  RocksDBRestWalHandler.cpp)

target_link_libraries(arango_rocksdb_rest
  PUBLIC
    arango_rocksdb_core
    arango_rocksdb_indexes
    arango_rocksdb_replication
    arango_common_rest_handler
    arango_restart_action)

add_library(arango_rocksdb_aql STATIC
  RocksDBOptimizerRules.cpp)

target_link_libraries(arango_rocksdb_aql
  PUBLIC arango_rocksdb_core)

if (USE_V8)
  add_library(arango_rocksdb_v8 STATIC RocksDBV8Functions.cpp)
  target_link_libraries(arango_rocksdb_v8 PUBLIC arango_rocksdb_core)
endif()

add_library(arango_rocksdb INTERFACE)
target_link_libraries(arango_rocksdb INTERFACE
  arango_rocksdb_core
  arango_rocksdb_indexes
  arango_rocksdb_replication
  arango_rocksdb_dump
  arango_rocksdb_rest
  arango_rocksdb_aql)
if (USE_V8)
  target_link_libraries(arango_rocksdb INTERFACE arango_rocksdb_v8)
endif()

set(_rocksdb_libs
  arango_rocksdb_core
  arango_rocksdb_indexes
  arango_rocksdb_replication
  arango_rocksdb_dump
  arango_rocksdb_rest
  arango_rocksdb_aql)
if (USE_V8)
  list(APPEND _rocksdb_libs arango_rocksdb_v8)
endif()

set(SNAPPY_INCLUDE_DIR "${SNAPPY_SOURCE_DIR};${SNAPPY_BUILD_DIR}" CACHE PATH "relation to snappy")

foreach(_lib ${_rocksdb_libs})
  target_include_directories(${_lib} PRIVATE
    "${PROJECT_SOURCE_DIR}/arangod"
    "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")
  target_include_directories(${_lib} PUBLIC ${SNAPPY_INCLUDE_DIR})

  if ((CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG) AND ARCH_AMD64)
    target_compile_options(${_lib} PUBLIC "-maes")
  endif()

  if(CMAKE_SYSTEM_NAME MATCHES "Cygwin")
    target_compile_options(${_lib} PRIVATE -fno-builtin-memcmp -DCYGWIN)
  elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    target_compile_options(${_lib} PRIVATE -DOS_MACOSX)
  elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
    target_compile_options(${_lib} PRIVATE -DOS_LINUX)
  elseif(CMAKE_SYSTEM_NAME MATCHES "SunOS")
    target_compile_options(${_lib} PRIVATE -DOS_SOLARIS)
  elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
    target_compile_options(${_lib} PRIVATE -DOS_FREEBSD)
  elseif(CMAKE_SYSTEM_NAME MATCHES "NetBSD")
    target_compile_options(${_lib} PRIVATE -DOS_NETBSD)
  elseif(CMAKE_SYSTEM_NAME MATCHES "OpenBSD")
    target_compile_options(${_lib} PRIVATE -DOS_OPENBSD)
  elseif(CMAKE_SYSTEM_NAME MATCHES "DragonFly")
    target_compile_options(${_lib} PRIVATE -DOS_DRAGONFLYBSD)
  elseif(CMAKE_SYSTEM_NAME MATCHES "Android")
    target_compile_options(${_lib} PRIVATE -DOS_ANDROID)
  elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
    target_compile_options(${_lib} PRIVATE -DWIN32 -DOS_WIN -D_MBCS -DWIN64 -DNOMINMAX)
    if(MINGW)
      target_compile_options(${_lib} PRIVATE -D_WIN32_WINNT=_WIN32_WINNT_VISTA)
    endif()
  endif()

  if (USE_ENTERPRISE)
    target_include_directories(${_lib} PUBLIC ${IRESEARCH_INCLUDE})
    target_link_libraries(${_lib} PUBLIC arango_rclone)
  endif()
endforeach()

add_subdirectory(Listeners)
add_subdirectory(Methods)