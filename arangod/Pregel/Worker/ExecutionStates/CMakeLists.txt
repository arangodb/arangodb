target_sources(arango_pregel_with_actors PRIVATE
        InitialState.cpp
        LoadingState.cpp
        LoadedState.cpp
        ComputingState.cpp
        StoringState.cpp
        StoredState.cpp
        ProducingResultsState.cpp
        ResultsProducedState.cpp
        CleaningUpState.cpp)
