add_subdirectory(EnumeratePathsFilter)

target_sources(arango_aql PRIVATE
  OptimizerRuleBatchMaterializeDocuments.cpp
  OptimizerRuleCollectWithIndex.cpp
  OptimizerRulePropagateConstantAttributes.cpp
  OptimizerRulePushDownLateMaterialization.cpp
  OptimizerRulePushLimitIntoIndex.cpp
  OptimizerRuleReplaceEntriesWithObjectIteration.cpp
  OptimizerRuleReplaceEqualAttributeAccess.cpp
  OptimizerRuleVectorIndex.cpp
  OptimizerRulesCluster.cpp
  OptimizerRulesGraph.cpp
  OptimizerRulesIResearchView.cpp
  OptimizerRulesIndexNode.cpp
  OptimizerRulesJoin.cpp
  OptimizerRulesLateMaterializedCommon.cpp
  OptimizerRulesReplaceFunctions.cpp
)
