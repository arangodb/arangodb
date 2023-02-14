target_sources(arango_pregel PRIVATE
  ConnectedComponents.cpp
  HITS.cpp
  HITSKleinberg.cpp
  LabelPropagation.cpp
  LineRank.cpp
  PageRank.cpp
  RecoveringPageRank.cpp
  ColorPropagation.cpp)

if (USE_MAINTAINER_MODE)
  target_sources(arango_pregel PRIVATE
    ReadWrite.cpp)
endif()

add_subdirectory(DMID)
add_subdirectory(EffectiveCloseness)
add_subdirectory(SCC)
add_subdirectory(ShortestPath)
add_subdirectory(SLPA)
add_subdirectory(SSSP)
add_subdirectory(WCC)
