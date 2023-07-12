target_sources(arango_replication2_pure PRIVATE
  ILogPersistor.h
  IStatePersistor.h
  IStorageEngineMethods.h
  LogStorageMethods.cpp
  PersistedStateInfo.h
  )

add_subdirectory(RocksDB)
add_subdirectory(WAL)
