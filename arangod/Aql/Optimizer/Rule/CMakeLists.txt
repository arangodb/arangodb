add_subdirectory(EnumeratePathsFilter)

target_sources(arango_aql PRIVATE
  BatchMaterializeDocuments.cpp
  CollectWithIndex.cpp
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
  ReplaceFunctions.cpp
)
