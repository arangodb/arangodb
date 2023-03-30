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
