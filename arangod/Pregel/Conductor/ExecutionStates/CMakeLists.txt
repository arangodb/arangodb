target_sources(arango_pregel_with_actors PRIVATE
  InitialState.cpp
  CreateWorkersState.cpp
  LoadingState.cpp
  ComputingState.cpp
  StoringState.cpp
  ProduceAQLResultsState.cpp
	AQLResultsAvailableState.cpp
  DoneState.cpp
  CanceledState.cpp
  FatalErrorState.cpp)
