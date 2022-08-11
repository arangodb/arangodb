target_sources(arango_pregel PRIVATE
  AsyncSCC.cpp
  ConnectedComponents.cpp
  HITS.cpp
  LabelPropagation.cpp
  LineRank.cpp
  PageRank.cpp
  RecoveringPageRank.cpp
  SCC.cpp
  SLPA.cpp
  SSSP.cpp
  ShortestPath.cpp
  WCC.cpp)

add_subdirectory(DMID)
add_subdirectory(EffectiveCloseness)
add_subdirectory(AIR)
