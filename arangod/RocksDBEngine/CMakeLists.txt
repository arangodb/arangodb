# -*- mode: CMAKE; -*-

# add sources for rocksdb engine
set(ROCKSDB_SOURCES
  RocksDBEngine/RocksDBCollection.cpp
  RocksDBEngine/RocksDBCommon.cpp
  RocksDBEngine/RocksDBComparator.cpp
  RocksDBEngine/RocksDBEdgeIndex.cpp
  RocksDBEngine/RocksDBEngine.cpp
  RocksDBEngine/RocksDBIndex.cpp
  RocksDBEngine/RocksDBIndexFactory.cpp
  RocksDBEngine/RocksDBKey.cpp
  RocksDBEngine/RocksDBKeyBounds.cpp
  RocksDBEngine/RocksDBPathBasedIndex.cpp
  RocksDBEngine/RocksDBPrimaryIndex.cpp
  RocksDBEngine/RocksDBPrimaryMockIndex.cpp
  RocksDBEngine/RocksDBTransactionCollection.cpp
  RocksDBEngine/RocksDBTransactionState.cpp
  RocksDBEngine/RocksDBTypes.cpp
  RocksDBEngine/RocksDBValue.cpp
  RocksDBEngine/RocksDBView.cpp
)
set(ROCKSDB_SOURCES ${ROCKSDB_SOURCES} PARENT_SCOPE)
