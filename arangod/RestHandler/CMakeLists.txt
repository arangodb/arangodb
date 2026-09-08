add_library(arango_common_rest_handler STATIC
  RestCollectionHandler.cpp
  RestReplicationHandler.cpp)

target_link_libraries(arango_common_rest_handler
  arango_cluster_methods
  arango_utils
  arango_iresearch)

target_include_directories(arango_common_rest_handler PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")

################################################################################
## OpenAPI spec embedding
################################################################################

# Generate hex-encoded OpenAPI specification files
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/openapi-v0.csx
  COMMAND ${CMAKE_COMMAND}
    -DSRC="${PROJECT_SOURCE_DIR}/OpenAPI/0-openapi.json"
    -DDST="${CMAKE_CURRENT_BINARY_DIR}/openapi-v0.csx"
    -P "${PROJECT_SOURCE_DIR}/lib/iresearch/cmake/HexEncodeFile.cmake"
  DEPENDS ${PROJECT_SOURCE_DIR}/OpenAPI/0-openapi.json
  COMMENT "Hex-encoding OpenAPI v0 specification"
)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/openapi-v1.csx
  COMMAND ${CMAKE_COMMAND}
    -DSRC="${PROJECT_SOURCE_DIR}/OpenAPI/1-openapi.json"
    -DDST="${CMAKE_CURRENT_BINARY_DIR}/openapi-v1.csx"
    -P "${PROJECT_SOURCE_DIR}/lib/iresearch/cmake/HexEncodeFile.cmake"
  DEPENDS ${PROJECT_SOURCE_DIR}/OpenAPI/1-openapi.json
  COMMENT "Hex-encoding OpenAPI v1 specification"
)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/openapi-v2.csx
  COMMAND ${CMAKE_COMMAND}
    -DSRC="${PROJECT_SOURCE_DIR}/OpenAPI/2-openapi.json"
    -DDST="${CMAKE_CURRENT_BINARY_DIR}/openapi-v2.csx"
    -P "${PROJECT_SOURCE_DIR}/lib/iresearch/cmake/HexEncodeFile.cmake"
  DEPENDS ${PROJECT_SOURCE_DIR}/OpenAPI/2-openapi.json
  COMMENT "Hex-encoding OpenAPI v2 specification"
)
# Create a custom target for the generated files
add_custom_target(
  openapi-hex-files
  DEPENDS
    ${CMAKE_CURRENT_BINARY_DIR}/openapi-v0.csx
    ${CMAKE_CURRENT_BINARY_DIR}/openapi-v1.csx
    ${CMAKE_CURRENT_BINARY_DIR}/openapi-v2.csx
)

add_library(arango_rest_handler STATIC
  RestAccessTokenHandler.cpp
  RestAdminClusterHandler.cpp
  RestAdminDatabaseHandler.cpp
  RestAdminDeploymentHandler.cpp
  RestAdminLogHandler.cpp
  RestAdminServerHandler.cpp
  RestAdminStatisticsHandler.cpp
  RestAqlFunctionsHandler.cpp
  RestAuthHandler.cpp
  RestAuthReloadHandler.cpp
  RestBaseHandler.cpp
  RestCompactHandler.cpp
  RestCrashHandler.cpp
  RestCursorHandler.cpp
  RestDatabaseHandler.cpp
  RestDebugHandler.cpp
  RestDocumentHandler.cpp
  RestDumpHandler.cpp
  RestEdgesHandler.cpp
  RestEndpointHandler.cpp
  RestEngineHandler.cpp
  RestExplainHandler.cpp
  RestImportHandler.cpp
  RestIndexHandler.cpp
  RestJobHandler.cpp
  RestKeyGeneratorsHandler.cpp
  RestLicenseHandler.cpp
  RestOpenApiHandler.cpp
  RestOptionsBaseHandler.cpp
  RestOptionsDescriptionHandler.cpp
  RestOptionsHandler.cpp
  RestPublicOptionsHandler.cpp
  RestQueryCacheHandler.cpp
  RestQueryHandler.cpp
  RestQueryPlanCacheHandler.cpp
  RestShutdownHandler.cpp
  RestSimpleHandler.cpp
  RestSimpleQueryHandler.cpp
  RestStatusHandler.cpp
  RestSupervisionStateHandler.cpp
  RestSupportInfoHandler.cpp
  RestSystemReportHandler.cpp
  RestTimeHandler.cpp
  RestTransactionHandler.cpp
  RestTtlHandler.cpp
  RestUploadHandler.cpp
  RestUsersHandler.cpp
  RestVersionHandler.cpp
  RestViewHandler.cpp
  RestVocbaseBaseHandler.cpp
  RestWalAccessHandler.cpp)

if(USE_V8)
  target_sources(arango_rest_handler PRIVATE
    RestAdminExecuteHandler.cpp
    RestAdminRoutingHandler.cpp
    RestAqlUserFunctionsHandler.cpp
    RestTasksHandler.cpp)
endif()

if(USE_MAINTAINER_MODE)
  target_sources(arango_rest_handler PRIVATE
    RestTestHandler.cpp)
endif()

target_link_libraries(arango_rest_handler
  PUBLIC
    arango_auth
    arango_actions
    arango_actor
    arango_basic_utils
    arango_statistics
    arango_iresearch
    arango::validation
    arango_v8
    arango_v8server
    rocksdb_interface
    immer
    velocypack)

if(USE_ENTERPRISE)
  target_link_libraries(arango_rest_handler
    PUBLIC
      arango_enterprise_audit
      arango_enterprise_rest_handler)
endif()

target_include_directories(arango_rest_handler PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}")

add_dependencies(arango_rest_handler openapi-hex-files)