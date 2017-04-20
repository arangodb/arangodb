# -*- mode: CMAKE; -*-

# add sources for rocksdb engine
set(ROCKSDB_SOURCES
  RocksDBEngine/RocksDBCollection.cpp
  RocksDBEngine/RocksDBCollectionExport.cpp
  RocksDBEngine/RocksDBCommon.cpp
  RocksDBEngine/RocksDBComparator.cpp
  RocksDBEngine/RocksDBCounterManager.cpp
  RocksDBEngine/RocksDBEdgeIndex.cpp
  RocksDBEngine/RocksDBEngine.cpp
  RocksDBEngine/RocksDBExportCursor.cpp
  RocksDBEngine/RocksDBIndex.cpp
  RocksDBEngine/RocksDBIndexFactory.cpp
  RocksDBEngine/RocksDBHashIndex.cpp
  RocksDBEngine/RocksDBKey.cpp
  RocksDBEngine/RocksDBKeyBounds.cpp
  RocksDBEngine/RocksDBPrimaryIndex.cpp
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
  RocksDBEngine/RocksDBVPackIndex.cpp
)
set(ROCKSDB_SOURCES ${ROCKSDB_SOURCES} PARENT_SCOPE)
