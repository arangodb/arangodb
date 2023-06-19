target_sources(arango_pregel PRIVATE
        VertexProcessor.cpp
        WorkerConfig.cpp
        Worker.cpp)

add_subdirectory(ExecutionStates)
