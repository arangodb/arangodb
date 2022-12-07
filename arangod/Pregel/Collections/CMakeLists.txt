target_sources(arango_pregel PRIVATE
  SingleServerCollection.cpp
  ClusterCollection.cpp
  CollectionFactory.cpp
  Collections.cpp)

add_subdirectory(Graph)
