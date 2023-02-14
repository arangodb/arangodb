target_sources(arango_pregel PRIVATE
  ConnectedComponents.cpp
  HITS.cpp
  ColorPropagation.cpp)

if (USE_MAINTAINER_MODE)
  add_subdirectory(ReadWrite)
endif()

add_subdirectory(DMID)
add_subdirectory(EffectiveCloseness)
add_subdirectory(HITSKleinberg)
add_subdirectory(LabelPropagation)
add_subdirectory(LineRank)
add_subdirectory(PageRank)
add_subdirectory(RecoveringPageRank)
add_subdirectory(SCC)
add_subdirectory(ShortestPath)
add_subdirectory(SLPA)
add_subdirectory(SSSP)
add_subdirectory(WCC)
