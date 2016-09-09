# -*- mode: CMAKE; -*-

include_directories(.)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/")

################################################################################
## BISON/FLEX
################################################################################

# note that we check-in the generated FLEX/BISON files, therefore they are
# generate inside the source tree

if (USE_MAINTAINER_MODE AND NOT MSVC)
  add_custom_command(
    OUTPUT
      ${CMAKE_CURRENT_SOURCE_DIR}/Aql/tokens.cpp
    WORKING_DIRECTORY
      ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND
      ${CMAKE_SOURCE_DIR}/utils/flex-c++.sh
      ${FLEX_EXECUTABLE}
      Aql/tokens.cpp
      Aql/tokens.ll
    MAIN_DEPENDENCY
      ${CMAKE_CURRENT_SOURCE_DIR}/Aql/tokens.ll
    VERBATIM
  )

  add_custom_command(
    OUTPUT
      ${CMAKE_CURRENT_SOURCE_DIR}/Aql/grammar.cpp
    WORKING_DIRECTORY
      ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND
      ${CMAKE_SOURCE_DIR}/utils/bison-c.sh
      ${BISON_EXECUTABLE}
      Aql/grammar.cpp
      Aql/grammar.y
    MAIN_DEPENDENCY
      ${CMAKE_CURRENT_SOURCE_DIR}/Aql/grammar.y
    VERBATIM
  )
endif ()

################################################################################
## arangod
################################################################################

if (MSVC)

  generate_product_version(ProductVersionFiles
    NAME arangod
    FILE_DESCRIPTION ${ARANGODB_FRIENDLY_STRING}
    ICON ${ARANGO_ICON}
    VERSION_MAJOR ${CPACK_PACKAGE_VERSION_MAJOR}
    VERSION_MINOR ${CPACK_PACKAGE_VERSION_MINOR}
    VERSION_PATCH ${CPACK_PACKAGE_VERSION_PATCH}
    VERSION_REVISION ${BUILD_ID}
  )
endif ()
  
if (USE_ROCKSDB) 
  set(ROCKSDB_FILES
    Indexes/RocksDBFeature.cpp
    Indexes/RocksDBIndex.cpp
    Indexes/RocksDBKeyComparator.cpp
  )
else ()
  set(ROCKSDB_FILES "")
endif ()

add_executable(${BIN_ARANGOD}
  ${ProductVersionFiles}
  ${ROCKSDB_FILES}
  Actions/ActionFeature.cpp
  Actions/RestActionHandler.cpp
  Actions/actions.cpp
  Agency/ActivationCallback.cpp
  Agency/AddFollower.cpp
  Agency/AgencyFeature.cpp
  Agency/Agent.cpp
  Agency/AgentActivator.cpp
  Agency/AgentCallback.cpp
  Agency/AgentConfiguration.cpp
  Agency/Constituent.cpp
  Agency/CleanOutServer.cpp
  Agency/FailedLeader.cpp
  Agency/FailedServer.cpp
  Agency/GossipCallback.cpp
  Agency/Inception.cpp
  Agency/MoveShard.cpp
  Agency/NotifyCallback.cpp
  Agency/Node.cpp
  Agency/Supervision.cpp
  Agency/RemoveServer.cpp
  Agency/RestAgencyHandler.cpp
  Agency/RestAgencyPrivHandler.cpp
  Agency/State.cpp
  Agency/Store.cpp
  Agency/StoreCallback.cpp
  Agency/UnassumedLeadership.cpp
  Agency/v8-agency.cpp
  Aql/Aggregator.cpp
  Aql/AqlItemBlock.cpp
  Aql/AqlItemBlockManager.cpp
  Aql/AqlValue.cpp
  Aql/Ast.cpp
  Aql/AstNode.cpp
  Aql/AttributeAccessor.cpp
  Aql/BasicBlocks.cpp
  Aql/BaseExpressionContext.cpp
  Aql/BindParameters.cpp
  Aql/CalculationBlock.cpp
  Aql/ClusterBlocks.cpp
  Aql/ClusterNodes.cpp
  Aql/CollectBlock.cpp
  Aql/CollectNode.cpp
  Aql/CollectOptions.cpp
  Aql/Collection.cpp
  Aql/CollectionScanner.cpp
  Aql/Collections.cpp
  Aql/Condition.cpp
  Aql/ConditionFinder.cpp
  Aql/EnumerateCollectionBlock.cpp
  Aql/EnumerateListBlock.cpp
  Aql/ExecutionBlock.cpp
  Aql/ExecutionEngine.cpp
  Aql/ExecutionNode.cpp
  Aql/ExecutionPlan.cpp
  Aql/ExecutionStats.cpp
  Aql/Executor.cpp
  Aql/Expression.cpp
  Aql/FixedVarExpressionContext.cpp
  Aql/FunctionDefinitions.cpp
  Aql/Function.cpp
  Aql/Functions.cpp
  Aql/Graphs.cpp
  Aql/IndexBlock.cpp
  Aql/IndexNode.cpp
  Aql/ModificationBlocks.cpp
  Aql/ModificationNodes.cpp
  Aql/ModificationOptions.cpp
  Aql/NodeFinder.cpp
  Aql/Optimizer.cpp
  Aql/OptimizerRules.cpp
  Aql/Parser.cpp
  Aql/Quantifier.cpp
  Aql/Query.cpp
  Aql/QueryCache.cpp
  Aql/QueryList.cpp
  Aql/QueryRegistry.cpp
  Aql/Range.cpp
  Aql/RestAqlHandler.cpp
  Aql/Scopes.cpp
  Aql/ShortestPathBlock.cpp
  Aql/ShortestPathNode.cpp
  Aql/ShortestPathOptions.cpp
  Aql/ShortStringStorage.cpp
  Aql/SortBlock.cpp
  Aql/SortCondition.cpp
  Aql/SortNode.cpp
  Aql/SubqueryBlock.cpp
  Aql/TraversalBlock.cpp
  Aql/TraversalConditionFinder.cpp
  Aql/TraversalNode.cpp
  Aql/V8Expression.cpp
  Aql/Variable.cpp
  Aql/VariableGenerator.cpp
  Aql/grammar.cpp
  Aql/tokens.cpp
  Cluster/AgencyCallback.cpp
  Cluster/AgencyCallbackRegistry.cpp
  Cluster/AgencyComm.cpp
  Cluster/ClusterComm.cpp
  Cluster/ClusterEdgeCursor.cpp
  Cluster/ClusterFeature.cpp
  Cluster/ClusterInfo.cpp
  Cluster/ClusterMethods.cpp
  Cluster/ClusterTraverser.cpp
  Cluster/DBServerAgencySync.cpp
  Cluster/HeartbeatThread.cpp
  Cluster/RestAgencyCallbacksHandler.cpp
  Cluster/RestShardHandler.cpp
  Cluster/TraverserEngine.cpp
  Cluster/TraverserEngineRegistry.cpp
  Cluster/ServerState.cpp
  Cluster/v8-cluster.cpp
  Dispatcher/Dispatcher.cpp
  Dispatcher/DispatcherFeature.cpp
  Dispatcher/DispatcherQueue.cpp
  Dispatcher/DispatcherThread.cpp
  Dispatcher/Job.cpp
  FulltextIndex/fulltext-handles.cpp
  FulltextIndex/fulltext-index.cpp
  FulltextIndex/fulltext-list.cpp
  FulltextIndex/fulltext-query.cpp
  FulltextIndex/fulltext-result.cpp
  GeneralServer/AsyncJobManager.cpp
  GeneralServer/GeneralCommTask.cpp
  GeneralServer/GeneralListenTask.cpp
  GeneralServer/GeneralServer.cpp
  GeneralServer/GeneralServerFeature.cpp
  GeneralServer/HttpCommTask.cpp
  GeneralServer/GeneralServerJob.cpp
  GeneralServer/HttpsCommTask.cpp
  GeneralServer/PathHandler.cpp
  GeneralServer/RestHandler.cpp
  GeneralServer/RestHandlerFactory.cpp
  GeneralServer/VppCommTask.cpp
  GeoIndex/GeoIndex.cpp
  Indexes/EdgeIndex.cpp
  Indexes/FulltextIndex.cpp
  Indexes/GeoIndex.cpp
  Indexes/HashIndex.cpp
  Indexes/Index.cpp
  Indexes/IndexIterator.cpp
  Indexes/PathBasedIndex.cpp
  Indexes/PrimaryIndex.cpp
  Indexes/SimpleAttributeEqualityMatcher.cpp
  Indexes/SkiplistIndex.cpp
  InternalRestHandler/InternalRestTraverserHandler.cpp
  ReadCache/CollectionRevisionCache.cpp
  ReadCache/GlobalRevisionCache.cpp
  ReadCache/GlobalRevisionCacheChunk.cpp
  ReadCache/RevisionReader.cpp
  Replication/ContinuousSyncer.cpp
  Replication/InitialSyncer.cpp
  Replication/Syncer.cpp
  RestHandler/RestAdminLogHandler.cpp
  RestHandler/RestAqlFunctionsHandler.cpp
  RestHandler/RestAuthHandler.cpp
  RestHandler/RestBaseHandler.cpp
  RestHandler/RestBatchHandler.cpp
  RestHandler/RestCursorHandler.cpp
  RestHandler/RestDebugHandler.cpp
  RestHandler/RestDocumentHandler.cpp
  RestHandler/RestEchoHandler.cpp
  RestHandler/RestEdgesHandler.cpp
  RestHandler/RestExportHandler.cpp
  RestHandler/RestImportHandler.cpp
  RestHandler/RestJobHandler.cpp
  RestHandler/RestPleaseUpgradeHandler.cpp
  RestHandler/RestQueryCacheHandler.cpp
  RestHandler/RestQueryHandler.cpp
  RestHandler/RestReplicationHandler.cpp
  RestHandler/RestShutdownHandler.cpp
  RestHandler/RestSimpleHandler.cpp
  RestHandler/RestSimpleQueryHandler.cpp
  RestHandler/RestUploadHandler.cpp
  RestHandler/RestVersionHandler.cpp
  RestHandler/RestVocbaseBaseHandler.cpp
  RestHandler/RestWalHandler.cpp
  RestHandler/WorkMonitorHandler.cpp
  RestServer/AffinityFeature.cpp
  RestServer/BootstrapFeature.cpp
  RestServer/CheckVersionFeature.cpp
  RestServer/ConsoleFeature.cpp
  RestServer/ConsoleThread.cpp
  RestServer/DatabaseFeature.cpp
  RestServer/DatabasePathFeature.cpp
  RestServer/EndpointFeature.cpp
  RestServer/FileDescriptorsFeature.cpp
  RestServer/FrontendFeature.cpp
  RestServer/InitDatabaseFeature.cpp
  RestServer/LockfileFeature.cpp
  RestServer/QueryRegistryFeature.cpp
  RestServer/ScriptFeature.cpp
  RestServer/ServerFeature.cpp
  RestServer/ServerIdFeature.cpp
  RestServer/TraverserEngineRegistryFeature.cpp
  RestServer/UnitTestsFeature.cpp
  RestServer/UpgradeFeature.cpp
  RestServer/VocbaseContext.cpp
  RestServer/WorkMonitorFeature.cpp
  RestServer/arangod.cpp
  Scheduler/ListenTask.cpp
  Scheduler/PeriodicTask.cpp
  Scheduler/Scheduler.cpp
  Scheduler/SchedulerFeature.cpp
  Scheduler/SchedulerLibev.cpp
  Scheduler/SchedulerThread.cpp
  Scheduler/SignalTask.cpp
  Scheduler/SocketTask.cpp
  Scheduler/Task.cpp
  Scheduler/TaskManager.cpp
  Scheduler/TimerTask.cpp
  Statistics/StatisticsFeature.cpp
  Statistics/statistics.cpp
  StorageEngine/EngineSelectorFeature.cpp
  StorageEngine/MMFilesCleanupThread.cpp
  StorageEngine/MMFilesCollection.cpp
  StorageEngine/MMFilesCompactorThread.cpp
  StorageEngine/MMFilesDatafileStatistics.cpp
  StorageEngine/MMFilesEngine.cpp
  StorageEngine/OtherEngine.cpp
  Utils/AqlTransaction.cpp
  Utils/CollectionExport.cpp
  Utils/CollectionKeys.cpp
  Utils/CollectionKeysRepository.cpp
  Utils/CollectionNameResolver.cpp
  Utils/Cursor.cpp
  Utils/CursorRepository.cpp
  Utils/OperationCursor.cpp
  Utils/SingleCollectionTransaction.cpp
  Utils/StandaloneTransactionContext.cpp
  Utils/Transaction.cpp
  Utils/TransactionContext.cpp
  Utils/V8TransactionContext.cpp
  Utils/WorkMonitorArangod.cpp
  V8Server/FoxxQueuesFeature.cpp
  V8Server/V8Context.cpp
  V8Server/V8DealerFeature.cpp
  V8Server/V8Job.cpp
  V8Server/V8PeriodicTask.cpp
  V8Server/V8QueueJob.cpp
  V8Server/V8TimerTask.cpp
  V8Server/V8Traverser.cpp
  V8Server/v8-actions.cpp
  V8Server/v8-collection-util.cpp
  V8Server/v8-collection.cpp
  V8Server/v8-dispatcher.cpp
  V8Server/v8-query.cpp
  V8Server/v8-replication.cpp
  V8Server/v8-statistics.cpp
  V8Server/v8-user-structures.cpp
  V8Server/v8-util.cpp
  V8Server/v8-vocbase.cpp
  V8Server/v8-voccursor.cpp
  V8Server/v8-vocindex.cpp
  VocBase/AuthInfo.cpp
  VocBase/DatafileStatisticsContainer.cpp
  VocBase/Ditch.cpp
  VocBase/EdgeCollectionInfo.cpp
  VocBase/Graphs.cpp
  VocBase/IndexPoolFeature.cpp
  VocBase/KeyGenerator.cpp
  VocBase/LogicalCollection.cpp
  VocBase/MasterPointers.cpp
  VocBase/PathEnumerator.cpp
  VocBase/SingleServerTraverser.cpp
  VocBase/Traverser.cpp
  VocBase/TraverserOptions.cpp
  VocBase/datafile.cpp
  VocBase/modes.cpp
  VocBase/replication-applier.cpp
  VocBase/replication-common.cpp
  VocBase/replication-dump.cpp
  VocBase/ticks.cpp
  VocBase/transaction.cpp
  VocBase/vocbase.cpp
  Wal/AllocatorThread.cpp
  Wal/CollectorThread.cpp
  Wal/Logfile.cpp
  Wal/LogfileManager.cpp
  Wal/RecoverState.cpp
  Wal/RecoveryFeature.cpp
  Wal/RemoverThread.cpp
  Wal/Slot.cpp
  Wal/Slots.cpp
  Wal/SynchronizerThread.cpp
  ${ADDITIONAL_BIN_ARANGOD_SOURCES}
)

target_link_libraries(${BIN_ARANGOD}
  ${LIB_ARANGO_FE}
  ${LIB_ARANGO_V8}
  ${LIB_ARANGO}
  ${LIBEV_LIBS}
  ${LINENOISE_LIBS}
  ${MSVC_LIBS}
  ${V8_LIBS}
  ${ROCKSDB_LIBS}
  ${SYSTEM_LIBRARIES}
)

install(
  TARGETS ${BIN_ARANGOD}
  RUNTIME DESTINATION ${CMAKE_INSTALL_SBINDIR})

install_config(arangod)

if (NOT USE_PRECOMPILED_V8)
  add_dependencies(arangod ev zlibstatic v8_build)
else ()
  add_dependencies(arangod ev zlibstatic)
endif ()

################################################################################
## arango-dfdb
################################################################################

install_command_alias(${BIN_ARANGOD}
  ${CMAKE_INSTALL_SBINDIR}
  arango-dfdb)

install_config(arango-dfdb)

################################################################################
## arango-secure-installation
################################################################################

install_command_alias(${BIN_ARANGOD}
  ${CMAKE_INSTALL_SBINDIR}
  arango-secure-installation)

install_config(arango-secure-installation)

################################################################################
## arango-init-database
################################################################################

install_command_alias(${BIN_ARANGOD}
  ${CMAKE_INSTALL_SBINDIR}
  arango-init-database)

install_config(arango-init-database)
