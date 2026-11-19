target_sources(arango_aql PRIVATE
  AccessesCollectionVariable.cpp
  ConditionCoverage.cpp
  ConditionFinder.cpp
  CalculationNodeVarFinder.cpp
  EnumeratePathsFilterMatcher.cpp
  ExtractNonConstPartsOfIndexCondition.cpp
  FindProjections.cpp
  GetBestIndexHandleForFilterCondition.cpp
  GetCollection.cpp
  GetIndexForSortCondition.cpp
  LateMaterializedCommon.cpp
  OptimizeTraversalPathVariable.cpp
  RewriteProjectionAttributeAccesses.cpp
  TraversalConditionFinder.cpp
)
