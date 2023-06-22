target_sources(arango_replication2 PRIVATE
  AgencySpecificationInspectors.cpp
  Algorithms.cpp
  ILogInterfaces.cpp
  NetworkAttachedFollower.cpp
  NetworkMessages.cpp
  ReplicatedLog.cpp
  SupervisionAgencyTrx.cpp
  ReplicatedLogFeature.cpp
  ReplicatedLogMetrics.tpp
  DefaultRebootIdCache.cpp DefaultRebootIdCache.h
  )

target_sources(arango_replication2_pure PRIVATE
  LogCommon.cpp
  LogLeader.cpp
  LogStatus.cpp
  types.cpp
  InMemoryLog.cpp
  WaitForBag.cpp
  LogEntries.cpp
  ReplicatedLog.cpp
  Supervision.cpp
  SupervisionAction.cpp

  AgencyLogSpecification.cpp
  Algorithms.cpp
  ILogInterfaces.cpp
  NetworkMessages.cpp
  ReplicatedLogMetrics.tpp
  TermIndexMapping.cpp
  IRebootIdCache.h
  Components/AppendEntriesManager.cpp Components/AppendEntriesManager.h
  Components/CompactionManager.cpp Components/CompactionManager.h
  Components/ExclusiveBool.h
  Components/FollowerCommitManager.cpp Components/FollowerCommitManager.h
  Components/IAppendEntriesManager.h
  Components/ICompactionManager.h
  Components/IFollowerCommitManager.h
  Components/IInMemoryLogManager.h
  Components/IMessageIdManager.h
  Components/IMethodsProvider.h
  Components/ISnapshotManager.h
  Components/IStateHandleManager.h
  Components/IStorageManager.h
  Components/InMemoryLogManager.cpp Components/InMemoryLogManager.h
  Components/LogFollower.cpp Components/LogFollower.h
  Components/MessageIdManager.cpp Components/MessageIdManager.h
  Components/MethodsProvider.cpp Components/MethodsProvider.h
  Components/SnapshotManager.cpp Components/SnapshotManager.h
  Components/StateHandleManager.cpp Components/StateHandleManager.h
  Components/StorageManager.cpp Components/StorageManager.h
  )
