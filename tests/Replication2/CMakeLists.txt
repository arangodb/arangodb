set(ARANGODB_REPLICATION2_TEST_HELPER_SOURCES
        Helper/ModelChecker/Actors.cpp
        Helper/ModelChecker/AgencyTransition.cpp
        #Helper/WaitForQueue.cpp
        #Mocks/AsyncFollower.cpp
        #Mocks/AsyncFollower.h
        #Mocks/AsyncLeader.cpp
        #Mocks/FakeAbstractFollower.h
        #Mocks/FakeFailureOracle.cpp
        #Mocks/FakeFailureOracle.h
        Mocks/FakeFollower.cpp
        Mocks/FakeFollower.h
        #Mocks/FakeLeader.cpp
        #Mocks/FakeLeader.h
        #Mocks/FakeReplicatedLog.cpp
        #Mocks/FakeReplicatedLog.h
        #Mocks/FakeReplicatedState.cpp
        #Mocks/PersistedLog.cpp
        #Mocks/PersistedLog.cpp
        #Mocks/PersistedLog.h
        #Mocks/PersistedLog.h
        #Mocks/ReplicatedStateMetricsMock.cpp
        ModelChecker/ModelChecker.cpp
        ModelChecker/Tests/ModelCheckerTest.cpp
        #ReplicatedLog/TestHelper.cpp
        Mocks/FakeStorageEngineMethods.cpp
        Mocks/FakeAsyncExecutor.cpp
        Mocks/LogEventRecorder.cpp
        )

set(ARANGODB_REPLICATION2_TEST_SOURCES
        #ReplicatedLog/AppendEntriesBatchTest.cpp
        ReplicatedLog/Algorithms/CalcCommitIndexTest.cpp
        #ReplicatedLog/ChangeStreamTests.cpp
        #ReplicatedLog/ConcurrencyTests.cpp
        ReplicatedLog/Algorithms/DetectConflictTest.cpp
        #ReplicatedLog/EstablishLeadershipTest.cpp
        #ReplicatedLog/FollowerAppendEntries.cpp
        #ReplicatedLog/FollowerWaitFor.cpp
        ReplicatedLog/Components/InMemoryLogTest.cpp
        #ReplicatedLog/LeaderAppendEntriesTest.cpp
        #ReplicatedLog/LogFollowerCompactionTest.cpp
        #ReplicatedLog/LogParticipantWaitForResignTests.cpp
        #ReplicatedLog/LogReclaimTest.cpp
        #ReplicatedLog/LogStatusTest.cpp
        #ReplicatedLog/MaintenanceTests.cpp
        #ReplicatedLog/MetricsTest.cpp
        #ReplicatedLog/MultiTermTest.cpp
        ReplicatedLog/Supervision/ParticipantsHealthTest.cpp
        #ReplicatedLog/ReplicatedLogHandleTest.cpp
        #ReplicatedLog/ReplicatedLogTest.cpp
        #ReplicatedLog/RewriteLogTest.cpp
        #ReplicatedLog/SimpleInsertTests.cpp
        ReplicatedLog/Supervision/SupervisionSimulationTest.cpp
        ReplicatedLog/Supervision/SupervisionTest.cpp
        ReplicatedLog/StorageEngineMethodsTest.cpp
        #ReplicatedLog/UpdateParticipantsFlags.cpp
        #ReplicatedLog/UpdateReplicatedLogTests.cpp
        #ReplicatedLog/WaitForSyncTest.cpp
        ReplicatedState/DocumentStateMachineTest.cpp
        #ReplicatedState/FollowerResignTest.cpp
        #ReplicatedState/FollowerSnapshotTest.cpp
        #ReplicatedState/FollowerWaitForTest.cpp
        #ReplicatedState/LeaderRecoveryTest.cpp
        #ReplicatedState/LeaderResignTest.cpp
        #ReplicatedState/PrototypeConcurrencyTest.cpp
        #ReplicatedState/PrototypeStateMachineTest.cpp
        #ReplicatedState/ReplicatedStateFeatureTest.cpp
        #ReplicatedState/ReplicatedStateMetricsTest.cpp
        #ReplicatedState/ReplicatedStateTest.cpp
        #ReplicatedState/StateCleanupTest.cpp
        #ReplicatedState/StateMachines/MyStateMachine.cpp
        ReplicatedLog/MaintenanceTests.cpp

        ReplicatedLog/Components/CompactionManagerTest.cpp
        ReplicatedLog/Components/StorageManagerTest.cpp
        ReplicatedLog/Components/FollowerCommitManagerTest.cpp
        ReplicatedLog/LogFollower/AppendEntriesTest.cpp
        )

add_library(arango_tests_replication2 OBJECT
        ${ARANGODB_REPLICATION2_TEST_SOURCES}
        ${ARANGODB_REPLICATION2_TEST_HELPER_SOURCES}
        )

target_include_directories(arango_tests_replication2 PUBLIC
        ${PROJECT_SOURCE_DIR}/arangod
        ${PROJECT_SOURCE_DIR}/lib
        # TODO: The below can be removed if Replication2 tests don't include relative to Replication2 anymore
        ${PROJECT_SOURCE_DIR}/tests
        ${PROJECT_SOURCE_DIR}/tests/Mocks/)

target_link_libraries(arango_tests_replication2
        gtest
        gmock
        arango
        arango_replication2
        velocypack
        fmt)

add_executable(arangodbtests_replication2
        EXCLUDE_FROM_ALL)

target_link_libraries(arangodbtests_replication2
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
        ReplicatedLog/Components/CompactionManagerTest.cpp
        ReplicatedLog/Components/StorageManagerTest.cpp
        ReplicatedLog/Components/FollowerCommitManagerTest.cpp
        ReplicatedLog/LogFollower/AppendEntriesTest.cpp
        ReplicatedLog/LogCommonTest.cpp
        ReplicatedLog/ReplicatedLogConnectTest.cpp
        )

target_sources(arango_tests_replication2_pure PRIVATE
        Mocks/ReplicatedLogMetricsMock.cpp
        Mocks/FakeStorageEngineMethods.cpp
        Mocks/FakeAsyncExecutor.cpp
        Mocks/LogEventRecorder.cpp)

target_link_libraries(arango_tests_replication2_pure
        arango_replication2_pure
        arango_lightweight
        arango_crashhandler_light
        gtest
        gmock
        immer
        rocksdb_interface
        )

target_include_directories(arango_tests_replication2_pure PRIVATE "${PROJECT_SOURCE_DIR}/tests")

add_executable(arangodbtests_replication2_pure
        EXCLUDE_FROM_ALL main-replication2.cpp)

target_link_libraries(arangodbtests_replication2_pure PRIVATE
        arango_tests_replication2_pure arango_lightweight)
