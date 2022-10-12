target_sources(arango_replication2 PRIVATE
  DocumentCore.cpp
  DocumentFollowerState.cpp
  DocumentLeaderState.cpp
  DocumentLogEntry.cpp
  DocumentStateAgencyHandler.cpp
  DocumentStateHandlersFactory.cpp
  DocumentStateMachine.cpp
  DocumentStateMachineFeature.cpp
  DocumentStateNetworkHandler.cpp
  DocumentStateShardHandler.cpp
  DocumentStateTransaction.cpp
  DocumentStateTransactionHandler.cpp
  DocumentStateMethods.cpp)
