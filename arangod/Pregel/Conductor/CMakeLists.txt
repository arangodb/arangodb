target_sources(arango_pregel PRIVATE
        ClusterWorkerApi.cpp
        Conductor.cpp
        PregelConductorFeature.cpp
        SingleServerWorkerApi.cpp)

add_subdirectory(States)
