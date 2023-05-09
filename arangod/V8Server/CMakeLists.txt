add_library(arango_v8server STATIC
  FoxxFeature.cpp
  GlobalContextMethods.cpp
  V8Context.cpp
  V8DealerFeature.cpp
  v8-actions.cpp
  v8-analyzers.cpp
  v8-collection-util.cpp
  v8-collection.cpp
  v8-dispatcher.cpp
  v8-general-graph.cpp
  v8-pregel.cpp
  v8-query.cpp
  v8-replicated-logs.cpp
  v8-replication.cpp
  v8-statistics.cpp
  v8-ttl.cpp
  v8-user-functions.cpp
  v8-user-structures.cpp
  v8-users.cpp
  v8-util.cpp
  v8-views.cpp
  v8-vocbase.cpp
  v8-voccursor.cpp
  v8-vocindex.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Cluster/v8-cluster.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Transaction/V8Context.cpp)

target_link_libraries(arango_v8server
  arango_agency
  arango_iresearch
  arango_replication
  arango_v8
  arango_v8_no_deadline
  ${V8_LIBS})

target_include_directories(arango_v8server PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")
