target_sources(arango_replication2 PRIVATE
  AgencySpecification.cpp
  FollowerStateManager.tpp
  LeaderStateManager.tpp
  PersistedStateInfo.cpp
  ReplicatedStateFeature.cpp
  ReplicatedStateMetrics.cpp
  ReplicatedStateToken.cpp
  StateCommon.cpp
  StateStatus.cpp
  Supervision.cpp
  SupervisionAction.cpp
  UnconfiguredStateManager.tpp
  UpdateReplicatedState.cpp
  UpdateReplicatedState.h)
