add_subdirectory(EnumeratePathsFilter)

target_sources(arango_aql PRIVATE
  OptimizerRuleBatchMaterializeDocuments.cpp
  OptimizerRulePropagateConstantAttributes.cpp
  OptimizerRulePushDownLateMaterialization.cpp
  OptimizerRuleReplaceEqualAttributeAccess.cpp
  OptimizerRulesCluster.cpp
  OptimizerRulesGraph.cpp
  OptimizerRulesIResearchView.cpp
  OptimizerRulesIndexNode.cpp
  OptimizerRulesJoin.cpp
  OptimizerRulesLateMaterializedCommon.cpp
  OptimizerRulesReplaceFunctions.cpp
)
