set(ARANGODB_REPLICATION2_TEST_HELPER_SOURCES
  #Helper/WaitForQueue.cpp
  #Mocks/AsyncFollower.cpp
  #Mocks/AsyncFollower.h
  #Mocks/AsyncLeader.cpp
  #Mocks/FakeAbstractFollower.h
  Mocks/FakeFollower.cpp
  Mocks/FakeFollower.h
  #Mocks/FakeLeader.cpp
  #Mocks/FakeLeader.h
  #Mocks/FakeReplicatedLog.cpp
  #Mocks/FakeReplicatedLog.h
  #Mocks/PersistedLog.cpp
  #Mocks/PersistedLog.cpp
  #Mocks/PersistedLog.h
  #Mocks/PersistedLog.h
  #ReplicatedLog/TestHelper.cpp

  Mocks/ParticipantsFactoryMock.h
  Mocks/FakeReplicatedState.cpp Mocks/FakeReplicatedState.h
  Mocks/ReplicatedStateMetricsMock.cpp Mocks/ReplicatedStateMetricsMock.h

  Mocks/DocumentStateMocks.cpp Mocks/DocumentStateMocks.h
  Mocks/MockVocbase.h
  Mocks/LeaderCommunicatorMock.h
  Mocks/StorageManagerMock.h
  Mocks/StateHandleManagerMock.h)


set(ARANGODB_REPLICATION2_TEST_SOURCES
  ReplicatedLog/Integration/MaintenanceTests.cpp
  ReplicatedLog/Integration/StorageEngineMethodsTest.cpp
  ReplicatedState/StateManagerTest.cpp
  ReplicatedState/StateMachines/DocumentState/CoreTest.cpp
  ReplicatedState/StateMachines/DocumentState/FollowerTest.cpp
  ReplicatedState/StateMachines/DocumentState/LeaderTest.cpp
  ReplicatedState/StateMachines/DocumentState/ShardHandlerTest.cpp
  ReplicatedState/StateMachines/DocumentState/SnapshotTest.cpp
  ReplicatedState/StateMachines/DocumentState/TransactionHandlerTest.cpp

  CollectionGroups/CollectionGroupsSupervision.cpp)

add_library(arango_tests_replication2 OBJECT
  ${ARANGODB_REPLICATION2_TEST_SOURCES}
  ${ARANGODB_REPLICATION2_TEST_HELPER_SOURCES}
  )

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # turn off annoying warning in test files
    target_compile_options(arango_tests_replication2 PRIVATE -Wno-missing-field-initializers)
endif ()

target_include_directories(arango_tests_replication2 PUBLIC
  ${PROJECT_SOURCE_DIR}/arangod
  ${PROJECT_SOURCE_DIR}/lib
  # TODO: The below can be removed if Replication2 tests don't include relative to Replication2 anymore
  ${PROJECT_SOURCE_DIR}/tests
  ${PROJECT_SOURCE_DIR}/tests/Mocks/)

target_link_libraries(arango_tests_replication2 PRIVATE
  gtest
  gmock
  arango
  arango_replication2
  arango_futures
  velocypack
  fmt)

option(USE_SEPARATE_REPLICATION2_TESTS_BINARY
  "Write a separate binary 'arangodbtests_replication2', containing only the Replication 2.0 tests.")
if (USE_SEPARATE_REPLICATION2_TESTS_BINARY)
    add_executable(arangodbtests_replication2)
else ()
    add_executable(arangodbtests_replication2
      EXCLUDE_FROM_ALL)
endif ()

target_link_libraries(arangodbtests_replication2 PRIVATE
  gtest
  arango_tests_basics
  arango_tests_replication2
  arango_agency
  arango_cluster_engine
  arango_rocksdb
  arango_v8server
  arangoserver
  boost_boost
  fmt)

add_test(NAME replication2
  COMMAND arangodbtests_replication2)


add_library(arango_tests_replication2_pure OBJECT
  ReplicatedLog/Algorithms/CalcCommitIndexTest.cpp
  ReplicatedLog/Algorithms/DetectConflictTest.cpp
  ReplicatedLog/Algorithms/LogCommonTest.cpp
  ReplicatedLog/Components/CompactionManagerTest.cpp
  ReplicatedLog/Components/FollowerCommitManagerTest.cpp
  ReplicatedLog/Components/InMemoryLogTest.cpp
  ReplicatedLog/Components/SnapshotManagerTest.cpp
  ReplicatedLog/Components/StorageManagerTest.cpp
  ReplicatedLog/Components/TermIndexMapping.cpp
  ReplicatedLog/LogFollower/AppendEntriesTest.cpp
  ReplicatedLog/ReplicatedLogConnectTest.cpp
  ReplicatedLog/Supervision/ParticipantsHealthTest.cpp
  ReplicatedLog/Supervision/SupervisionSimulationTest.cpp
  ReplicatedLog/Supervision/SupervisionTest.cpp
  ReplicatedLog/Components/SnapshotManagerTest.cpp
  )


target_sources(arango_tests_replication2_pure PRIVATE
  Mocks/ReplicatedLogMetricsMock.cpp
  Mocks/FakeStorageEngineMethods.cpp
  Mocks/FakeAsyncExecutor.cpp
  Helper/ModelChecker/Actors.cpp
  Helper/ModelChecker/AgencyTransition.cpp

  ModelChecker/ModelChecker.cpp
  ModelChecker/Tests/ModelCheckerTest.cpp
  )

target_link_libraries(arango_tests_replication2_pure PUBLIC
  arango_replication2_pure
  arango_lightweight
  arango_crashhandler_light
  arango_futures
  gtest
  gmock
  immer
  rocksdb_interface
  boost_boost
  )

target_include_directories(arango_tests_replication2_pure PRIVATE "${PROJECT_SOURCE_DIR}/tests")

add_executable(arangodbtests_replication2_pure
  EXCLUDE_FROM_ALL main-replication2.cpp)

target_link_libraries(arangodbtests_replication2_pure PRIVATE
  arango_tests_replication2_pure arango_lightweight)

add_test(NAME replication2_pure
  COMMAND arangodbtests_replication2_pure)
target_link_libraries(arangodbtests_replication2 PUBLIC arango_tests_replication2_pure)
