# -*- mode: CMAKE; -*-

##
## these defines are copied from rocksdb's CMakeLists.txt
##  because we are currently using some of their internal headers
##  in our build
# add definitions for various operating systems
if(CMAKE_SYSTEM_NAME MATCHES "Cygwin")
  add_definitions(-fno-builtin-memcmp -DCYGWIN)
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
  add_definitions(-DOS_MACOSX)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES arm)
    add_definitions(-DIOS_CROSS_COMPILE -DROCKSDB_LITE)
    # no debug info for IOS, that will make our library big
    add_definitions(-DNDEBUG)
  endif()
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
  add_definitions(-DOS_LINUX)
elseif(CMAKE_SYSTEM_NAME MATCHES "SunOS")
  add_definitions(-DOS_SOLARIS)
elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
  add_definitions(-DOS_FREEBSD)
elseif(CMAKE_SYSTEM_NAME MATCHES "NetBSD")
  add_definitions(-DOS_NETBSD)
elseif(CMAKE_SYSTEM_NAME MATCHES "OpenBSD")
  add_definitions(-DOS_OPENBSD)
elseif(CMAKE_SYSTEM_NAME MATCHES "DragonFly")
  add_definitions(-DOS_DRAGONFLYBSD)
elseif(CMAKE_SYSTEM_NAME MATCHES "Android")
  add_definitions(-DOS_ANDROID)
elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
  add_definitions(-DWIN32 -DOS_WIN -D_MBCS -DWIN64 -DNOMINMAX)
  if(MINGW)
    add_definitions(-D_WIN32_WINNT=_WIN32_WINNT_VISTA)
  endif()
endif()



# add sources for rocksdb engine
set(ROCKSDB_SOURCES
  RocksDBEngine/Listeners/RocksDBBackgroundErrorListener.cpp
  RocksDBEngine/Listeners/RocksDBMetricsListener.cpp
  RocksDBEngine/Listeners/RocksDBShaCalculator.cpp
  RocksDBEngine/Listeners/RocksDBThrottle.cpp
  RocksDBEngine/RocksDBBackgroundThread.cpp
  RocksDBEngine/RocksDBBuilderIndex.cpp
  RocksDBEngine/RocksDBCollection.cpp
  RocksDBEngine/RocksDBColumnFamilyManager.cpp
  RocksDBEngine/RocksDBCommon.cpp
  RocksDBEngine/RocksDBComparator.cpp
  RocksDBEngine/RocksDBCuckooIndexEstimator.cpp
  RocksDBEngine/RocksDBEdgeIndex.cpp
  RocksDBEngine/RocksDBEngine.cpp
  RocksDBEngine/RocksDBFormat.cpp
  RocksDBEngine/RocksDBFulltextIndex.cpp
  RocksDBEngine/RocksDBGeoIndex.cpp
  RocksDBEngine/RocksDBIncrementalSync.cpp
  RocksDBEngine/RocksDBIndex.cpp
  RocksDBEngine/RocksDBIndexFactory.cpp
  RocksDBEngine/RocksDBIterators.cpp
  RocksDBEngine/RocksDBKey.cpp
  RocksDBEngine/RocksDBKeyBounds.cpp
  RocksDBEngine/RocksDBLogValue.cpp
  RocksDBEngine/RocksDBMetaCollection.cpp
  RocksDBEngine/RocksDBMetadata.cpp
  RocksDBEngine/RocksDBMethods.cpp
  RocksDBEngine/RocksDBOptimizerRules.cpp
  RocksDBEngine/RocksDBOptionFeature.cpp
  RocksDBEngine/RocksDBPrimaryIndex.cpp
  RocksDBEngine/RocksDBRecoveryManager.cpp
  RocksDBEngine/RocksDBReplicationCommon.cpp
  RocksDBEngine/RocksDBReplicationContext.cpp
  RocksDBEngine/RocksDBReplicationIterator.cpp
  RocksDBEngine/RocksDBReplicationManager.cpp
  RocksDBEngine/RocksDBReplicationTailing.cpp
  RocksDBEngine/RocksDBRestCollectionHandler.cpp
  RocksDBEngine/RocksDBRestExportHandler.cpp
  RocksDBEngine/RocksDBRestHandlers.cpp
  RocksDBEngine/RocksDBRestReplicationHandler.cpp
  RocksDBEngine/RocksDBRestWalHandler.cpp
  RocksDBEngine/RocksDBSavePoint.cpp
  RocksDBEngine/RocksDBSettingsManager.cpp
  RocksDBEngine/RocksDBSyncThread.cpp
  RocksDBEngine/RocksDBTransactionCollection.cpp
  RocksDBEngine/RocksDBTransactionState.cpp
  RocksDBEngine/RocksDBTtlIndex.cpp
  RocksDBEngine/RocksDBTypes.cpp
  RocksDBEngine/RocksDBUpgrade.cpp
  RocksDBEngine/RocksDBV8Functions.cpp
  RocksDBEngine/RocksDBVPackIndex.cpp
  RocksDBEngine/RocksDBValue.cpp
  RocksDBEngine/RocksDBWalAccess.cpp
)
set(ROCKSDB_SOURCES ${ROCKSDB_SOURCES} PARENT_SCOPE)
