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

if (USE_MRUBY)
  set(ARANGOD_MRUBY_SOURCE
      MRServer/ApplicationMR.cpp
      MRServer/mr-actions.cpp
  )

  set(ARANGOD_MRUBY_LIBS
      ${LIB_ARANGO_MRUBY}
      ${MRUBY_LIBS}
  )
endif ()

add_executable(
    ${BIN_ARANGOD}
    Actions/actions.cpp
    Actions/RestActionHandler.cpp
    Ahuacatl/ahuacatl-access-optimiser.c
    Ahuacatl/ahuacatl-ast-node.c
    Ahuacatl/ahuacatl-bind-parameter.c
    Ahuacatl/ahuacatl-codegen.c
    Ahuacatl/ahuacatl-collections.c
    Ahuacatl/ahuacatl-context.c
    Ahuacatl/ahuacatl-conversions.c
    Ahuacatl/ahuacatl-error.c
    Ahuacatl/ahuacatl-explain.c
    Ahuacatl/ahuacatl-functions.c
    Ahuacatl/ahuacatl-grammar.c
    Ahuacatl/ahuacatl-index.c
    Ahuacatl/ahuacatl-node.c
    Ahuacatl/ahuacatl-optimiser.c
    Ahuacatl/ahuacatl-parser-functions.c
    Ahuacatl/ahuacatl-parser.c
    Ahuacatl/ahuacatl-result.c
    Ahuacatl/ahuacatl-scope.c
    Ahuacatl/ahuacatl-statement-dump.c
    Ahuacatl/ahuacatl-statement-walker.c
    Ahuacatl/ahuacatl-statementlist.c
    Ahuacatl/ahuacatl-tokens.c
    Ahuacatl/ahuacatl-variable.c
    BitIndexes/bitarray.c
    BitIndexes/bitarrayIndex.c
    CapConstraint/cap-constraint.c
    FulltextIndex/fulltext-handles.c
    FulltextIndex/fulltext-index.c
    FulltextIndex/fulltext-list.c
    FulltextIndex/fulltext-query.c
    FulltextIndex/fulltext-result.c
    FulltextIndex/fulltext-wordlist.c
    GeoIndex/GeoIndex.c
    GeoIndex/geo-index.c
    HashIndex/hash-array.c
    HashIndex/hash-index.c
    IndexIterators/index-iterator.c
    IndexOperators/index-operator.c
    PriorityQueue/pqueueindex.c
    PriorityQueue/priorityqueue.c
    Replication/replication-static.cpp
    Replication/ContinuousSyncer.cpp
    Replication/InitialSyncer.cpp
    Replication/Syncer.cpp
    RestHandler/RestBatchHandler.cpp
    RestHandler/RestDocumentHandler.cpp
    RestHandler/RestEdgeHandler.cpp
    RestHandler/RestImportHandler.cpp
    RestHandler/RestReplicationHandler.cpp
    RestHandler/RestUploadHandler.cpp
    RestHandler/RestVocbaseBaseHandler.cpp
    RestServer/ArangoServer.cpp
    RestServer/VocbaseContext.cpp
    RestServer/arango.cpp
    SkipLists/skiplist.c
    SkipLists/skiplistIndex.c
    SkipListsEx/skiplistEx.c
    SkipListsEx/skiplistExIndex.c
    Utils/DocumentHelper.cpp
    V8Server/ApplicationV8.cpp
    V8Server/v8-actions.cpp
    V8Server/v8-query.cpp
    V8Server/v8-vocbase.cpp
    VocBase/auth.c
    VocBase/barrier.c
    VocBase/cleanup.c
    VocBase/collection.c
    VocBase/compactor.c
    VocBase/datafile.c
    VocBase/document-collection.c
    VocBase/edge-collection.c
    VocBase/general-cursor.c
    VocBase/headers.c
    VocBase/index.c
    VocBase/index-garbage-collector.c
    VocBase/key-generator.c
    VocBase/marker.c
    VocBase/primary-collection.c
    VocBase/replication-applier.c
    VocBase/replication-common.c
    VocBase/replication-dump.c
    VocBase/replication-logger.c
    VocBase/replication-master.c
    VocBase/server.c
    VocBase/shape-collection.c
    VocBase/synchroniser.c
    VocBase/transaction.c
    VocBase/update-policy.c
    VocBase/voc-shaper.c
    VocBase/vocbase.c
    VocBase/vocbase-defaults.c
    ${ARANGOD_MRUBY_SOURCE}
)

target_link_libraries(
    ${BIN_ARANGOD}
    ${LIB_ARANGO_FE}
    ${LIB_ARANGO_V8}
    ${LIB_ARANGO}
    ${ARANGOD_MRUBY_LIBS}
    ${LIBEV_LIBS}
    ${V8_LIBS} # need this for rest::Version
    ${ICU_LIBS}
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
  DIRECTORY ${PROJECT_SOURCE_DIR}/js/common ${PROJECT_SOURCE_DIR}/js/server ${PROJECT_SOURCE_DIR}/js/actions
  DESTINATION share/arangodb/js
  FILES_MATCHING PATTERN "*.js"
  REGEX "^.*/common/test-data$" EXCLUDE
  REGEX "^.*/common/tests$" EXCLUDE
  REGEX "^.*/server/tests$" EXCLUDE)

install(
  DIRECTORY ${PROJECT_SOURCE_DIR}/js/npm
  DESTINATION share/arangodb/js
  REGEX ".travis.yml" EXCLUDE
  REGEX ".npmignore" EXCLUDE)

install(
  DIRECTORY ${PROJECT_SOURCE_DIR}/js/node ${PROJECT_SOURCE_DIR}/js/apps
  DESTINATION share/arangodb/js
  PATTERN "databases" EXCLUDE)

################################################################################
### @brief install arangod mruby files
################################################################################

install(
  DIRECTORY ${PROJECT_SOURCE_DIR}/mr/common ${PROJECT_SOURCE_DIR}/mr/server ${PROJECT_SOURCE_DIR}/mr/actions
  DESTINATION share/arangodb/mr
  FILES_MATCHING PATTERN "*.rb")

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
