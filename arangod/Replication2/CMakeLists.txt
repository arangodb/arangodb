add_library(arango_replication2 STATIC
  AgencyCollectionSpecification.cpp
  AgencyMethods.cpp
  Methods.cpp
  MetricsHelper.cpp
  Version.cpp
  ${PROJECT_SOURCE_DIR}/arangod/RestHandler/RestLogHandler.cpp
  ${PROJECT_SOURCE_DIR}/arangod/RestHandler/RestLogInternalHandler.cpp
  ${PROJECT_SOURCE_DIR}/arangod/RestHandler/RestPrototypeStateHandler.cpp
  ${PROJECT_SOURCE_DIR}/arangod/RestHandler/RestDocumentStateHandler.cpp ReplicatedLog/Components/LogFollower2.cpp ReplicatedLog/Components/LogFollower2.h ReplicatedLog/Components/StorageManager.cpp ReplicatedLog/Components/StorageManager.h ReplicatedLog/Components/CompactionManager.cpp ReplicatedLog/Components/CompactionManager.h ReplicatedLog/Components/SnapshotManager.cpp ReplicatedLog/Components/SnapshotManager.h ReplicatedLog/Components/AppendEntriesManager.cpp ReplicatedLog/Components/AppendEntriesManager.h ReplicatedLog/Components/ExclusiveBool.h ReplicatedLog/Components/FollowerCommitManager.cpp ReplicatedLog/Components/FollowerCommitManager.h)

target_link_libraries(arango_replication2
  arango
  arango_network
  arangoserver
  fmt
  immer)

target_include_directories(arango_replication2 PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")

target_compile_options(arango_replication2 PRIVATE
	${CMAKE_DEPRECATE_OPTIONS})

add_subdirectory(Exceptions)
add_subdirectory(ReplicatedLog)
add_subdirectory(ReplicatedState)
add_subdirectory(StateMachines)
add_subdirectory(Streams)
