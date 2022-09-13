target_sources(arango_pregel PRIVATE
  Conductor.cpp
  WorkerApi.cpp)

add_subdirectory(States)
