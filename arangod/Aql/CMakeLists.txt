add_library(arango_aql STATIC
  Aggregator.cpp
  AqlCall.cpp
  AqlCallList.cpp
  AqlCallSet.cpp
  AqlCallStack.cpp
  AqlExecuteResult.cpp
  AqlFunctionFeature.cpp
  AqlFunctionsInternalCache.cpp
  AqlItemBlock.cpp
  AqlItemBlockInputRange.cpp
  AqlItemBlockManager.cpp
  AqlItemBlockUtils.cpp
  AqlItemMatrix.cpp
  AqlTransaction.cpp
  AqlValue.cpp
  AqlValueGroup.cpp
  AqlValueMaterializer.cpp
  Arithmetic.cpp
  Ast.cpp
  AstHelper.cpp
  AstNode.cpp
  AstResources.cpp
  AsyncExecutor.cpp
  AttributeAccessor.cpp
  AttributeNamePath.cpp
  BindParameters.cpp
  BlocksWithClients.cpp
  CalculationExecutor.cpp
  CalculationNodeVarFinder.cpp
  ClusterNodes.cpp
  ClusterQuery.cpp
  CollectionAccess.cpp
  CollectionAccessingNode.cpp
  Collection.cpp
  Collections.cpp
  CollectNode.cpp
  CollectOptions.cpp
  Condition.cpp
  ConditionFinder.cpp
  ConstFetcher.cpp
  ConstrainedSortExecutor.cpp
  CostEstimate.cpp
  CountCollectExecutor.cpp
  DependencyProxy.cpp
  DistinctCollectExecutor.cpp
  DistributeClientBlock.cpp
  DistributeConsumerNode.cpp
  DistributeExecutor.cpp
  DocumentExpressionContext.cpp
  DocumentProducingHelper.cpp
  DocumentProducingNode.cpp
  EngineInfoContainerCoordinator.cpp
  EngineInfoContainerDBServerServerBased.cpp
  EnumerateCollectionExecutor.cpp
  EnumerateListExecutor.cpp
  EnumeratePathsExecutor.cpp
  EnumeratePathsNode.cpp
  ExecutionBlock.cpp
  ExecutionEngine.cpp
  ExecutionNode.cpp
  ExecutionNodeId.cpp
  ExecutionPlan.cpp
  ExecutionState.cpp
  ExecutionStats.cpp
  ExecutorExpressionContext.cpp
  Expression.cpp
  FilterExecutor.cpp
  FixedVarExpressionContext.cpp
  Function.cpp
  Functions.cpp
  grammar.cpp
  GraphNode.cpp
  Graphs.cpp
  HashedCollectExecutor.cpp
  IdExecutor.cpp
  InAndOutRowExpressionContext.cpp
  IndexExecutor.cpp
  IndexHint.cpp
  IndexNode.cpp
  IndexNodeOptimizerRules.cpp
  InputAqlItemRow.cpp
  InsertModifier.cpp
  IResearchViewExecutor.cpp
  IResearchViewNode.cpp
  IResearchViewOptimizerRules.cpp
  LateMaterializedExpressionContext.cpp
  LateMaterializedOptimizerRulesCommon.cpp
  LimitExecutor.cpp
  LimitStats.cpp
  MaterializeExecutor.cpp
  ModificationExecutor.cpp
  ModificationExecutorHelpers.cpp
  ModificationExecutorInfos.cpp
  ModificationNodes.cpp
  ModificationOptions.cpp
  MultiAqlItemBlockInputRange.cpp
  MultiDependencySingleRowFetcher.cpp
  MutexExecutor.cpp
  MutexNode.cpp
  NodeFinder.cpp
  NonConstExpressionContainer.cpp
  NonConstExpression.cpp
  NoResultsExecutor.cpp
  Optimizer.cpp
  OptimizerRulesCluster.cpp
  OptimizerRules.cpp
  OptimizerRulesFeature.cpp
  OptimizerRulesReplaceFunctions.cpp
  OptimizerUtils.cpp
  OutputAqlItemRow.cpp
  ParallelUnsortedGatherExecutor.cpp
  Parser.cpp
  Projections.cpp
  PruneExpressionEvaluator.cpp
  Quantifier.cpp
  QueryCache.cpp
  QueryContext.cpp
  Query.cpp
  QueryExecutionState.cpp
  QueryExpressionContext.cpp
  QueryList.cpp
  QueryOptions.cpp
  QueryProfile.cpp
  QueryRegistry.cpp
  QuerySnippet.cpp
  QueryString.cpp
  QueryWarnings.cpp
  Range.cpp
  RegisterId.cpp
  RegisterInfos.cpp
  RegisterPlan.cpp
  RemoteExecutor.cpp
  RemoveModifier.cpp
  RestAqlHandler.cpp
  ReturnExecutor.cpp
  ScatterExecutor.cpp
  Scopes.cpp
  ShadowAqlItemRow.cpp
  ShardLocking.cpp
  SharedAqlItemBlockPtr.cpp
  SharedQueryState.cpp
  ShortestPathExecutor.cpp
  ShortestPathNode.cpp
  ShortStringStorage.cpp
  SimpleModifier.cpp
  SingleRemoteModificationExecutor.cpp
  SingleRowFetcher.cpp
  SkipResult.cpp
  SortCondition.cpp
  SortedCollectExecutor.cpp
  SortedRowsStorageBackendMemory.cpp
  SortedRowsStorageBackendStaged.cpp
  SortExecutor.cpp
  SortingGatherExecutor.cpp
  SortNode.cpp
  SortRegister.cpp
  SubqueryEndExecutionNode.cpp
  SubqueryEndExecutor.cpp
  SubqueryStartExecutionNode.cpp
  SubqueryStartExecutor.cpp
  Timing.cpp
  tokens.cpp
  TraversalConditionFinder.cpp
  TraversalExecutor.cpp
  TraversalNode.cpp
  TraverserEngineShardLists.cpp
  UnsortedGatherExecutor.cpp
  UpdateReplaceModifier.cpp
  UpsertModifier.cpp
  V8Executor.cpp
  Variable.cpp
  VariableGenerator.cpp
  VarUsageFinder.cpp
  WindowExecutor.cpp
  WindowNode.cpp)

target_link_libraries(arango_aql
  arango_geo
  arango_graph
  arango_indexes
  arango_iresearch
  arango_network
  immer)

target_include_directories(arango_aql PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")

target_compile_options(arango_aql PRIVATE
	${CMAKE_DEPRECATE_OPTIONS})

################################################################################
## BISON/FLEX
################################################################################

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tokens.cpp
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${FLEX_EXECUTABLE} -L -otokens.cpp ${CMAKE_CURRENT_SOURCE_DIR}/tokens.ll
  MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/tokens.ll
  VERBATIM
)

# Find Git Bash on Windows. GITBASH_EXECUTABLE is not set for other OSes
if (MSVC)
  get_filename_component(GITBASH_EXECUTABLE "[HKEY_LOCAL_MACHINE\\SOFTWARE\\GitForWindows;InstallPath]/bin/bash.exe" ABSOLUTE)
  if (${GITBASH_EXECUTABLE} STREQUAL "/registry/bin/bash.exe")
    message(FATAL_ERROR "Could not find Git for Windows' bash.exe, which is needed to run bison-c.sh for generating the AQL grammar.cpp")
  endif()
endif()

add_custom_command(
  OUTPUT
    ${CMAKE_CURRENT_BINARY_DIR}/grammar.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/grammar.hpp
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${GITBASH_EXECUTABLE} ${CMAKE_SOURCE_DIR}/utils/bison-c.sh
    ${BISON_EXECUTABLE} grammar.cpp ${CMAKE_CURRENT_SOURCE_DIR}/grammar.y
  MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/grammar.y
  VERBATIM
)

if (CMAKE_COMPILER_IS_GNUCC)
  set_source_files_properties(tokens.cpp PROPERTIES COMPILE_FLAGS -Wno-sign-compare) # TODO: Set build directory here?
endif ()
