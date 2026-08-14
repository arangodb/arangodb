add_library(arangoserver INTERFACE)

target_link_libraries(arangoserver INTERFACE
        arango_actions
        arango_auth
        arango_cluster
        arango_feature_phases
        arango_general_server
        arango_rest_handler
        arango_rest_server
        arango_sharding
        arango_statistics
        arango_transaction
        arango_vector_index
        arango_system_monitor_activities
        arango_system_monitor_async_registry
        arango_agency
        arango_cluster_engine
        arango_cluster_engine_rest
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
        arango_rocksdb_rest
        arango_utils
        arango_vocbase
        arango_scheduler
        boost_boost
        ${MSVC_LIBS})

if(USE_ENTERPRISE)
  target_link_libraries(arangoserver INTERFACE
    arango_enterprise_audit
    arango_enterprise_license
    arango_enterprise_sharding
    arango_enterprise_ssl
    arango_enterprise_rest_handler
    arango_enterprise_storage_engine)
endif()

if(MSVC)
  target_link_libraries(arangoserver INTERFACE Bcrypt.lib)
endif()

if(USE_V8)
  target_link_libraries(arangoserver INTERFACE arango_v8server)
endif()

add_dependencies(arangoserver tzdata)
