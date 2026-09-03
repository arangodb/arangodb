target_sources(arango_aql PRIVATE
  AccessesCollectionVariable.cpp
  EnumeratePathsFilterMatcher.cpp
  ExtractNonConstPartsOfIndexCondition.cpp
  FindProjections.cpp
  GetBestIndexHandleForFilterCondition.cpp
  GetIndexForSortCondition.cpp
  LateMaterializedCommon.cpp
  OptimizeTraversalPathVariable.cpp
  RewriteProjectionAttributeAccesses.cpp
)
