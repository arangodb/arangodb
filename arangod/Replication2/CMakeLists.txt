add_library(arango_replication2 STATIC
        AgencyCollectionSpecification.cpp
        Supervision/CollectionGroupSupervision.cpp
        AgencyMethods.cpp
        Methods.cpp
        MetricsHelper.cpp
        Version.cpp
        ${PROJECT_SOURCE_DIR}/arangod/RestHandler/RestLogHandler.cpp
        ${PROJECT_SOURCE_DIR}/arangod/RestHandler/RestLogInternalHandler.cpp
        ${PROJECT_SOURCE_DIR}/arangod/RestHandler/RestDocumentStateHandler.cpp)


target_link_libraries(arango_replication2
        arango # -> V8
        arango_network # NetworkWorkAttachedFollower
        arangoserver # RestHandler, arango_rocksdb
        arango_futures
        fmt
        immer)

target_include_directories(arango_replication2 PRIVATE
        "${PROJECT_SOURCE_DIR}/arangod"
        "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")

add_library(arango_replication2_pure STATIC
  MetricsHelper.cpp
  IScheduler.h)

target_link_libraries(arango_replication2_pure PUBLIC arango_lightweight
        velocypack immer fmt arango_metrics_base arango_futures)

target_include_directories(arango_replication2_pure PUBLIC
        "${PROJECT_SOURCE_DIR}/arangod"
        "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")

target_link_libraries(arango_replication2
        arango_replication2_pure)

add_subdirectory(Exceptions)
add_subdirectory(ReplicatedLog)
add_subdirectory(ReplicatedState)
add_subdirectory(StateMachines)
add_subdirectory(Streams)
add_subdirectory(Storage)
