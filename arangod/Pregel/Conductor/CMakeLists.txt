target_sources(arango_pregel PRIVATE
  Conductor.cpp
  SingleServerWorkerApi.cpp
  ClusterWorkerApi.cpp)

add_subdirectory(States)
