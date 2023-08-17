target_sources(arango_replication2 PRIVATE
  ILogPersistor.h
  IStatePersistor.h
  IStorageEngineMethods.h
  LogStorageMethods.cpp
  PersistedStateInfo.h
  )

add_subdirectory(RocksDB)
