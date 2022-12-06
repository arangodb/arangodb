add_library(arango_replication2 STATIC
        AgencyCollectionSpecification.cpp
        AgencyMethods.cpp
        Methods.cpp
        MetricsHelper.cpp
        Version.cpp
        ${PROJECT_SOURCE_DIR}/arangod/RestHandler/RestLogHandler.cpp
        ${PROJECT_SOURCE_DIR}/arangod/RestHandler/RestLogInternalHandler.cpp
        ${PROJECT_SOURCE_DIR}/arangod/RestHandler/RestPrototypeStateHandler.cpp
        ${PROJECT_SOURCE_DIR}/arangod/RestHandler/RestDocumentStateHandler.cpp)




target_link_libraries(arango_replication2
  arango # -> V8
  arango_network # NetworkWorkAttachedFollower
  arangoserver # RestHandler, arango_rocksdb
  fmt
  immer)

target_include_directories(arango_replication2 PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")

target_compile_options(arango_replication2 PRIVATE
	${CMAKE_DEPRECATE_OPTIONS})

add_library(arango_replication2_pure STATIC
		MetricsHelper.cpp
		ReplicatedState/StateStatus.cpp
		ReplicatedState/StateCommon.cpp
		ReplicatedLog/NetworkMessages.cpp
		ReplicatedLog/Algorithms.cpp
		ReplicatedLog/LogCore.cpp
		ReplicatedLog/ILogInterfaces.cpp
		)

target_link_libraries(arango_replication2_pure PUBLIC arango_lightweight
		velocypack immer fmt arango_metrics_base)

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
