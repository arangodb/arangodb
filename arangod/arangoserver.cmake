# Source files from the 11 subdirs (Actions, Auth, Cluster, FeaturePhases,
# GeneralServer, RestHandler, RestServer, Sharding, Statistics, Transaction,
# VectorIndex) live in per-subdir CMakeLists.txt as separate archives
# (arango_auth, ...). arangoserver aggregates them here.
#
# Additional sources are still contributed via target_sources() from
# arangod/SystemMonitor/Activities/CMakeLists.txt and
# arangod/SystemMonitor/AsyncRegistry/CMakeLists.txt.
add_library(arangoserver STATIC)

target_link_libraries(arangoserver
  arango_auth
  arango_cluster
  arango_feature_phases
  arango_general_server
  arango_rest_handler
  arango_rest_server
  arango_sharding
  arango_transaction
  arango_vector_index
  arango_agency
  arango_aql
  arango_cluster_engine
  arango_cluster_methods
  arango_common_rest_handler
  arango_futures
  arango_geo
  arango_graph
  arango_indexes
  arango_inspection
  arango_iresearch
  arango_metrics
  arango_network
  arango_replication
  arango_storage_engine
  arango_utils
  arango_vocbase
  arango_scheduler
  boost_boost
  ${MSVC_LIBS})

target_include_directories(arangoserver PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")

add_dependencies(arangoserver tzdata)
