# -*- mode: CMAKE; -*-

# add sources for rocksdb engine
set(ROCKSDB_SOURCES
  RocksDBEngine/Indexes/RocksDBEdgeIndex.cpp
  RocksDBEngine/Indexes/RocksDBFulltextIndex.cpp
  RocksDBEngine/Indexes/RocksDBGeoIndex.cpp
  RocksDBEngine/Indexes/RocksDBGeoIndexImpl.cpp
  RocksDBEngine/Indexes/RocksDBHashIndex.cpp
  RocksDBEngine/Indexes/RocksDBIndex.cpp
  RocksDBEngine/Indexes/RocksDBIndexFactory.cpp
  RocksDBEngine/Indexes/RocksDBPrimaryIndex.cpp
  RocksDBEngine/Indexes/RocksDBVPackIndex.cpp
  RocksDBEngine/Indexes/S2GeoIndex.cpp
  RocksDBEngine/RocksDBAqlFunctions.cpp
  RocksDBEngine/RocksDBBackgroundThread.cpp
  RocksDBEngine/RocksDBCollection.cpp
  RocksDBEngine/RocksDBCommon.cpp
  RocksDBEngine/RocksDBComparator.cpp
  RocksDBEngine/RocksDBCounterManager.cpp
  RocksDBEngine/RocksDBEngine.cpp
  RocksDBEngine/RocksDBExportCursor.cpp
  RocksDBEngine/RocksDBIncrementalSync.cpp
  RocksDBEngine/RocksDBIterators.cpp
  RocksDBEngine/RocksDBKey.cpp
  RocksDBEngine/RocksDBKeyBounds.cpp
  RocksDBEngine/RocksDBLogValue.cpp
  RocksDBEngine/RocksDBMethods.cpp
  RocksDBEngine/RocksDBOptimizerRules.cpp
  RocksDBEngine/RocksDBReplicationCommon.cpp
  RocksDBEngine/RocksDBReplicationContext.cpp
  RocksDBEngine/RocksDBReplicationManager.cpp
  RocksDBEngine/RocksDBReplicationTailing.cpp
  RocksDBEngine/RocksDBRestExportHandler.cpp
  RocksDBEngine/RocksDBRestHandlers.cpp
  RocksDBEngine/RocksDBRestReplicationHandler.cpp
  RocksDBEngine/RocksDBRestWalHandler.cpp
  RocksDBEngine/RocksDBTransactionCollection.cpp
  RocksDBEngine/RocksDBTransactionState.cpp
  RocksDBEngine/RocksDBTypes.cpp
  RocksDBEngine/RocksDBV8Functions.cpp
  RocksDBEngine/RocksDBValue.cpp
  RocksDBEngine/RocksDBView.cpp
  RocksDBEngine/RocksDBWalAccess.cpp
)
set(ROCKSDB_SOURCES ${ROCKSDB_SOURCES} PARENT_SCOPE)
