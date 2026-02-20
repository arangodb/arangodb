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
