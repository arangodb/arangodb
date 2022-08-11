target_sources(arango_pregel PRIVATE
  InitialState.cpp
  LoadingState.cpp
  ComputingState.cpp
  StoringState.cpp
  CanceledState.cpp
  DoneState.cpp
  InErrorState.cpp
  RecoveringState.cpp
  FatalErrorState.cpp)
