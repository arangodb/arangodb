target_sources(arango_replication2 PRIVATE
  ILogPersistor.h
  IStatePersistor.h
  IStorageEngineMethods.h
  PersistedStateInfo.h
  )

add_subdirectory(RocksDB)