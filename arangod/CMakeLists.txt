# -*- mode: CMAKE; -*-

include_directories(.)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/")

################################################################################
## BISON/FLEX
################################################################################

# note that we check-in the generated FLEX/BISON files, therefore they are
# generate inside the source tree

if (USE_MAINTAINER_MODE)
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
  set(ARANGO_MSVC
    RestServer/WindowsServiceUtils.cpp
  )

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

add_executable(${BIN_ARANGOD}
  ${ARANGO_MSVC}
  ${ProductVersionFiles}
  Actions/RestActionHandler.cpp
  Actions/actions.cpp
  Agency/Agent.cpp
  Agency/ApplicationAgency.cpp
  Agency/Constituent.cpp
  Agency/State.cpp
  Agency/Store.cpp
  Agency/AgentCallback.cpp
  ApplicationServer/ApplicationFeature.cpp
  ApplicationServer/ApplicationServer.cpp
  Aql/Aggregator.cpp
  Aql/AqlItemBlock.cpp
  Aql/AqlItemBlockManager.cpp
  Aql/AqlValue.cpp
  Aql/Ast.cpp
  Aql/AstNode.cpp
  Aql/AttributeAccessor.cpp
  Aql/BasicBlocks.cpp
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
  Aql/Function.cpp
  Aql/Functions.cpp
  Aql/Graphs.cpp
  Aql/Index.cpp
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
  Cluster/AgencyComm.cpp
  Cluster/ApplicationCluster.cpp
  Cluster/ClusterComm.cpp
  Cluster/ClusterInfo.cpp
  Cluster/ClusterMethods.cpp
  Cluster/ClusterTraverser.cpp
  Cluster/HeartbeatThread.cpp
  Cluster/RestShardHandler.cpp
  Cluster/ServerJob.cpp
  Cluster/ServerState.cpp
  Cluster/v8-cluster.cpp
  Dispatcher/ApplicationDispatcher.cpp
  Dispatcher/Dispatcher.cpp
  Dispatcher/DispatcherQueue.cpp
  Dispatcher/DispatcherThread.cpp
  Dispatcher/Job.cpp
  FulltextIndex/fulltext-handles.cpp
  FulltextIndex/fulltext-index.cpp
  FulltextIndex/fulltext-list.cpp
  FulltextIndex/fulltext-query.cpp
  FulltextIndex/fulltext-result.cpp
  FulltextIndex/fulltext-wordlist.cpp
  GeoIndex/GeoIndex.cpp
  HttpServer/ApplicationEndpointServer.cpp
  HttpServer/AsyncJobManager.cpp
  HttpServer/HttpCommTask.cpp
  HttpServer/HttpHandler.cpp
  HttpServer/HttpHandlerFactory.cpp
  HttpServer/HttpListenTask.cpp
  HttpServer/HttpServer.cpp
  HttpServer/HttpServerJob.cpp
  HttpServer/HttpsCommTask.cpp
  HttpServer/HttpsServer.cpp
  HttpServer/PathHandler.cpp
  IndexOperators/index-operator.cpp
  Indexes/CapConstraint.cpp
  Indexes/EdgeIndex.cpp
  Indexes/FulltextIndex.cpp
  Indexes/GeoIndex2.cpp
  Indexes/HashIndex.cpp
  Indexes/Index.cpp
  Indexes/IndexIterator.cpp
  Indexes/PathBasedIndex.cpp
  Indexes/PrimaryIndex.cpp
  Indexes/SimpleAttributeEqualityMatcher.cpp
  Indexes/SkiplistIndex.cpp
  Replication/ContinuousSyncer.cpp
  Replication/InitialSyncer.cpp
  Replication/Syncer.cpp
  RestHandler/RestAdminLogHandler.cpp
  RestHandler/RestAgencyHandler.cpp
  RestHandler/RestAgencyPrivHandler.cpp
  RestHandler/RestBaseHandler.cpp
  RestHandler/RestBatchHandler.cpp
  RestHandler/RestCursorHandler.cpp
  RestHandler/RestDebugHandler.cpp
  RestHandler/RestDocumentHandler.cpp
  RestHandler/RestEdgeHandler.cpp
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
  RestHandler/WorkMonitorHandler.cpp
  RestServer/ArangoServer.cpp
  RestServer/ConsoleThread.cpp
  RestServer/VocbaseContext.cpp
  RestServer/arangod.cpp
  Scheduler/ApplicationScheduler.cpp
  Scheduler/ListenTask.cpp
  Scheduler/PeriodicTask.cpp
  Scheduler/Scheduler.cpp
  Scheduler/SchedulerLibev.cpp
  Scheduler/SchedulerThread.cpp
  Scheduler/SignalTask.cpp
  Scheduler/SocketTask.cpp
  Scheduler/Task.cpp
  Scheduler/TaskManager.cpp
  Scheduler/TimerTask.cpp
  Statistics/statistics.cpp
  Storage/Marker.cpp
  Storage/Options.cpp
  Utils/CollectionExport.cpp
  Utils/CollectionKeys.cpp
  Utils/CollectionKeysRepository.cpp
  Utils/Cursor.cpp
  Utils/CursorRepository.cpp
  Utils/DocumentHelper.cpp
  Utils/ShapedJsonTransformer.cpp
  Utils/StandaloneTransactionContext.cpp
  Utils/Transaction.cpp
  Utils/TransactionContext.cpp
  Utils/V8TransactionContext.cpp
  Utils/WorkMonitorArangod.cpp
  V8Server/ApplicationV8.cpp
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
  V8Server/v8-shape-conv.cpp
  V8Server/v8-statistics.cpp
  V8Server/v8-user-structures.cpp
  V8Server/v8-util.cpp
  V8Server/v8-vocbase.cpp
  V8Server/v8-voccursor.cpp
  V8Server/v8-vocindex.cpp
  V8Server/v8-wrapshapedjson.cpp
  VocBase/DatafileStatistics.cpp
  VocBase/Ditch.cpp
  VocBase/DocumentAccessor.cpp
  VocBase/ExampleMatcher.cpp
  VocBase/Graphs.cpp
  VocBase/KeyGenerator.cpp
  VocBase/Legends.cpp
  VocBase/Shaper.cpp
  VocBase/Traverser.cpp
  VocBase/VocShaper.cpp
  VocBase/auth.cpp
  VocBase/cleanup.cpp
  VocBase/collection.cpp
  VocBase/compactor.cpp
  VocBase/datafile.cpp
  VocBase/document-collection.cpp
  VocBase/edge-collection.cpp
  VocBase/headers.cpp
  VocBase/replication-applier.cpp
  VocBase/replication-common.cpp
  VocBase/replication-dump.cpp
  VocBase/server.cpp
  VocBase/shape-accessor.cpp
  VocBase/shaped-json.cpp
  VocBase/transaction.cpp
  VocBase/vocbase-defaults.cpp
  VocBase/vocbase.cpp
  Wal/AllocatorThread.cpp
  Wal/CollectorThread.cpp
  Wal/Logfile.cpp
  Wal/LogfileManager.cpp
  Wal/Marker.cpp
  Wal/RecoverState.cpp
  Wal/RemoverThread.cpp
  Wal/Slot.cpp
  Wal/Slots.cpp
  Wal/SynchronizerThread.cpp
)

target_link_libraries(${BIN_ARANGOD}
  ${LIB_ARANGO_FE}
  ${LIB_ARANGO_V8}
  ${LIB_ARANGO}
  ${LIBEV_LIBS}
  ${LINENOISE_LIBS}
  ${MSVC_LIBS}
  ${V8_LIBS}
  ${SYSTEM_LIBRARIES}
)

install(
  TARGETS ${BIN_ARANGOD}
  RUNTIME DESTINATION ${ARANGODB_INSTALL_SBIN})

install_config(arangod)

add_dependencies(arangod ev zlibstatic v8_build)

################################################################################
## arango-dfdb
################################################################################

install_command_alias(${BIN_ARANGOD}
  ${ARANGODB_INSTALL_SBIN}
  arango-dfdb)

install_config(arango-dfdb)
