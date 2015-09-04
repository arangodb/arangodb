# -*- mode: CMAKE; -*-

## -----------------------------------------------------------------------------
## --SECTION--                                                   COMMON INCLUDES
## -----------------------------------------------------------------------------

################################################################################
### @brief local directory
################################################################################

include_directories(.)

################################################################################
### @brief library source
################################################################################

include_directories(${PROJECT_SOURCE_DIR}/lib)

## -----------------------------------------------------------------------------
## --SECTION--                                                       EXECUTABLES
## -----------------------------------------------------------------------------

################################################################################
### @brief output directory
################################################################################

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")

################################################################################
### @brief arangod
################################################################################

if (MSVC)
  SET(ARANGO_MSVC
    RestServer/WindowsServiceUtils.cpp
    )
  ################################################################################
  ### @brief give the binary a version information
  ################################################################################

  generate_product_version(
    ProductVersionFiles
    NAME arangod
    FILE_DESCRIPTION ${ARANGODB_FRIENDLY_STRING}
    ICON ${ARANGO_ICON}
    VERSION_MAJOR ${CPACK_PACKAGE_VERSION_MAJOR}
    VERSION_MINOR ${CPACK_PACKAGE_VERSION_MINOR}
    VERSION_PATCH ${CPACK_PACKAGE_VERSION_PATCH}
    VERSION_REVISION ${BUILD_ID}
    )
endif ()


add_executable(
    ${BIN_ARANGOD}
    ${ARANGO_MSVC}
    ${ProductVersionFiles}
    Actions/actions.cpp
    Actions/RestActionHandler.cpp
    Admin/ApplicationAdminServer.cpp
    ApplicationServer/ApplicationFeature.cpp
    ApplicationServer/ApplicationServer.cpp
    Aql/AggregateBlock.cpp
    Aql/AggregationOptions.cpp
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
    Aql/Collection.cpp
    Aql/CollectionScanner.cpp
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
    Aql/grammar.cpp
    Aql/IndexRangeBlock.cpp
    Aql/ModificationBlock.cpp
    Aql/NodeFinder.cpp
    Aql/Optimizer.cpp
    Aql/OptimizerRules.cpp
    Aql/Parser.cpp
    Aql/Query.cpp
    Aql/QueryCache.cpp
    Aql/QueryList.cpp
    Aql/QueryRegistry.cpp
    Aql/RangeInfo.cpp
    Aql/Range.cpp
    Aql/RestAqlHandler.cpp
    Aql/Scopes.cpp
    Aql/ShortStringStorage.cpp
    Aql/SortBlock.cpp
    Aql/SubqueryBlock.cpp
    Aql/tokens.cpp
    Aql/V8Expression.cpp
    Aql/Variable.cpp
    Aql/VariableGenerator.cpp
    Aql/ModificationOptions.cpp
    Cluster/AgencyComm.cpp
    Cluster/ApplicationCluster.cpp
    Cluster/ClusterComm.cpp
    Cluster/ClusterInfo.cpp
    Cluster/ClusterMethods.cpp
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
    Dispatcher/RequeueTask.cpp
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
    Indexes/CapConstraint.cpp
    Indexes/EdgeIndex.cpp
    Indexes/FulltextIndex.cpp
    Indexes/GeoIndex2.cpp
    Indexes/HashIndex.cpp
    Indexes/Index.cpp
    Indexes/PathBasedIndex.cpp
    Indexes/PrimaryIndex.cpp
    Indexes/SkiplistIndex.cpp
    IndexOperators/index-operator.cpp
    Replication/ContinuousSyncer.cpp
    Replication/InitialSyncer.cpp
    Replication/Syncer.cpp
    Rest/AnyServer.cpp
    RestHandler/RestAdminBaseHandler.cpp
    RestHandler/RestAdminLogHandler.cpp
    RestHandler/RestBaseHandler.cpp
    RestHandler/RestBatchHandler.cpp
    RestHandler/RestDebugHelperHandler.cpp
    RestHandler/RestCursorHandler.cpp
    RestHandler/RestDocumentHandler.cpp
    RestHandler/RestEdgeHandler.cpp
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
    Utils/CollectionExport.cpp
    Utils/Cursor.cpp
    Utils/CursorRepository.cpp
    Utils/DocumentHelper.cpp
    Utils/StandaloneTransactionContext.cpp
    Utils/Transaction.cpp
    Utils/TransactionContext.cpp
    Utils/V8TransactionContext.cpp
    V8Server/ApplicationV8.cpp
    V8Server/V8Job.cpp
    V8Server/V8PeriodicTask.cpp
    V8Server/V8QueueJob.cpp
    V8Server/V8TimerTask.cpp
    V8Server/V8Traverser.cpp
    V8Server/v8-actions.cpp
    V8Server/v8-collection.cpp
    V8Server/v8-collection-util.cpp
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
    VocBase/auth.cpp
    VocBase/cleanup.cpp
    VocBase/collection.cpp
    VocBase/compactor.cpp
    VocBase/datafile.cpp
    VocBase/Ditch.cpp
    VocBase/document-collection.cpp
    VocBase/ExampleMatcher.cpp
    VocBase/edge-collection.cpp
    VocBase/headers.cpp
    VocBase/KeyGenerator.cpp
    VocBase/Legends.cpp
    VocBase/replication-applier.cpp
    VocBase/replication-common.cpp
    VocBase/replication-dump.cpp
    VocBase/replication-master.cpp
    VocBase/server.cpp
    VocBase/shape-accessor.cpp
    VocBase/shaped-json.cpp
    VocBase/Shaper.cpp
    VocBase/transaction.cpp
    VocBase/vocbase.cpp
    VocBase/vocbase-defaults.cpp
    VocBase/VocShaper.cpp
    Wal/AllocatorThread.cpp
    Wal/CollectorThread.cpp
    Wal/LogfileManager.cpp
    Wal/Logfile.cpp
    Wal/Marker.cpp
    Wal/RecoverState.cpp
    Wal/RemoverThread.cpp
    Wal/Slot.cpp
    Wal/Slots.cpp
    Wal/SynchronizerThread.cpp
)

target_link_libraries(
    ${BIN_ARANGOD}
    ${LIB_ARANGO_FE}
    ${LIB_ARANGO_V8}
    ${LIB_ARANGO}
    ${LIBEV_LIBS}
    ${V8_LIBS} # need this for rest::Version
    ${ICU_LIBS}
    ${BT_LIBS}
    ${ZLIB_LIBS}
    ${READLINE_LIBS}
    ${OPENSSL_LIBS}
    ${CMAKE_THREAD_LIBS_INIT}
    ${MSVC_LIBS}
)

################################################################################
### @brief install arangod binary
################################################################################

install(
  TARGETS ${BIN_ARANGOD}
  RUNTIME DESTINATION ${TRI_SBINDIR_INSTALL})

################################################################################
### @brief install arangod config
################################################################################

install_config(arangod)

################################################################################
### @brief install arango-dfdb binary
################################################################################

install_command_alias(${BIN_ARANGOD} ${TRI_SBINDIR_INSTALL} arango-dfdb)

################################################################################
### @brief install arango-dfdb config
################################################################################

install_config(arango-dfdb)

################################################################################
### @brief install server-side JavaScript files
################################################################################

install(
  DIRECTORY ${PROJECT_BINARY_DIR}/js
  DESTINATION share/arangodb)

################################################################################
### @brief install log directory
################################################################################

install(
  DIRECTORY ${PROJECT_BINARY_DIR}/var/log/arangodb
  DESTINATION ${VARDIR_INSTALL}/log)

################################################################################
### @brief install database directory
################################################################################

install(
  DIRECTORY ${PROJECT_BINARY_DIR}/var/lib/arangodb
  DESTINATION ${VARDIR_INSTALL}/lib)

################################################################################
### @brief install apps directory
################################################################################

install(
  DIRECTORY ${PROJECT_BINARY_DIR}/var/lib/arangodb-apps
  DESTINATION ${VARDIR_INSTALL}/lib)

## -----------------------------------------------------------------------------
## --SECTION--                                                       END-OF-FILE
## -----------------------------------------------------------------------------

## Local Variables:
## mode: outline-minor
## outline-regexp: "### @brief\\|## --SECTION--\\|# -\\*- "
## End:
