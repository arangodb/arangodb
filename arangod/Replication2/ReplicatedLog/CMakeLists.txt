target_sources(arango_replication2 PRIVATE
        AgencySpecificationInspectors.cpp
        Algorithms.cpp
        ILogInterfaces.cpp
        NetworkAttachedFollower.cpp
        NetworkMessages.cpp
        ReplicatedLog.cpp
        SupervisionAgencyTrx.cpp
        ReplicatedLogFeature.cpp
        ReplicatedLogMetrics.tpp)

target_sources(arango_replication2_pure PRIVATE
        LogCommon.cpp
        LogLeader.cpp
        LogStatus.cpp
        types.cpp
        InMemoryLog.cpp
        WaitForBag.cpp
        LogEntries.cpp
        ReplicatedLog.cpp
        Supervision.cpp
        SupervisionAction.cpp

        ReplicatedLogMetrics.tpp
        AgencyLogSpecification.cpp
        Components/LogFollower2.cpp
        Components/LogFollower2.h
        Components/StorageManager.cpp
        Components/StorageManager.h
        Components/CompactionManager.cpp
        Components/CompactionManager.h
        Components/SnapshotManager.cpp
        Components/SnapshotManager.h
        Components/AppendEntriesManager.cpp
        Components/AppendEntriesManager.h
        Components/ExclusiveBool.h
        Components/FollowerCommitManager.cpp
        Components/FollowerCommitManager.h
        Components/StateHandleManager.cpp
        Components/StateHandleManager.h)
