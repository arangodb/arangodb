add_subdirectory(EnumeratePathsFilter)

target_sources(arango_aql PRIVATE
  AsyncPrefetch.cpp
  BatchMaterializeDocuments.cpp
  CollectWithIndex.cpp
  DecayUnnecessarySortedGather.cpp
  PropagateConstantAttributes.cpp
  PushDownLateMaterialization.cpp
  PushLimitIntoIndex.cpp
  ReplaceEntriesWithObjectIteration.cpp
  ReplaceEqualAttributeAccess.cpp
  VectorIndex.cpp
  PushFilterVectorIndex.cpp
  Cluster.cpp
  Graph.cpp
  IResearchView.cpp
  IndexNode.cpp
  Join.cpp
  LateMaterializedCommon.cpp
  OptimizeCount.cpp
  OptimizeProjections.cpp
  ReplaceFunctions.cpp
  SpliceSubqueries.cpp
)
