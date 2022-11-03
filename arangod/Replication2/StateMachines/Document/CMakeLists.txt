target_sources(arango_replication2 PRIVATE
  ActiveTransactionsQueue.cpp
  DocumentCore.cpp
  DocumentFollowerState.cpp
  DocumentLeaderState.cpp
  DocumentLogEntry.cpp
  DocumentStateAgencyHandler.cpp
  DocumentStateHandlersFactory.cpp
  DocumentStateMachine.cpp
  DocumentStateMachineFeature.cpp
  DocumentStateMethods.cpp
  DocumentStateNetworkHandler.cpp
  DocumentStateShardHandler.cpp
  DocumentStateSnapshot.cpp
  DocumentStateTransaction.cpp
  DocumentStateTransactionHandler.cpp)
