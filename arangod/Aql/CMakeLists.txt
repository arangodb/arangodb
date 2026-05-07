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
  AqlTransaction.cpp
  AqlValue.cpp
  AqlValueGroup.cpp
  AqlValueMaterializer.cpp
  Arithmetic.cpp
  Ast.cpp
  AstHelper.cpp
  AstNode.cpp
  AstResources.cpp
  AsyncPrefetchSlotsManager.cpp
  AttributeAccessor.cpp
  AttributeNamePath.cpp
  BindParameters.cpp
  BlocksWithClients.cpp
  CalculationNodeVarFinder.cpp
  ClusterQuery.cpp
  CollectOptions.cpp
  Collection.cpp
  CollectionAccess.cpp
  Collections.cpp
  Condition.cpp
  ConditionPart.cpp
  ConditionCoverage.cpp
  ConditionFinder.cpp
  ConstFetcher.cpp
  CostEstimate.cpp
  DependencyProxy.cpp
  DistributeClientBlock.cpp
  DocumentExpressionContext.cpp
  DocumentProducingHelper.cpp
  EdgeConditionBuilder.cpp
  EngineInfoContainerCoordinator.cpp
  EngineInfoContainerDBServerServerBased.cpp
  ExecutionBlock.cpp
  ExecutionEngine.cpp
  ExecutionNodeId.cpp
  ExecutionPlan.cpp
  ExecutionState.cpp
  ExecutionStats.cpp
  ExecutorExpressionContext.cpp
  Expression.cpp
  FixedVarExpressionContext.cpp
  Function.cpp
  Functions.cpp
  InAndOutRowExpressionContext.cpp
  IndexHint.cpp
  IndexJoinStrategy.cpp
  InputAqlItemRow.cpp
  InsertModifier.cpp
  LateMaterializedExpressionContext.cpp
  LazyConditions.cpp
  LimitStats.cpp
  ModificationExecutorFlags.h
  ModificationOptions.cpp
  MultiAqlItemBlockInputRange.cpp
  MultiDependencySingleRowFetcher.cpp
  NodeFinder.cpp
  NonConstExpression.cpp
  NonConstExpressionContainer.cpp
  Optimizer.cpp
  OptimizerRulesFeature.cpp
  OptimizerRulesOptionsProvider.cpp
  OptimizerUtils.cpp
  OutputAqlItemRow.cpp
  Projections.cpp
  PruneExpressionEvaluator.cpp
  Quantifier.cpp
  Query.cpp
  QueryCache.cpp
  QueryContext.cpp
  QueryExecutionState.cpp
  QueryExpressionContext.cpp
  QueryInfoLoggerFeature.cpp
  QueryList.cpp
  QueryOptions.cpp
  QueryPlanCache.cpp
  QueryProfile.cpp
  QueryRegistry.cpp
  QuerySnippet.cpp
  QueryString.cpp
  QueryWarnings.cpp
  Range.cpp
  RegisterId.cpp
  RegisterInfos.cpp
  RegisterPlan.cpp
  RemoveModifier.cpp
  RestAqlHandler.cpp
  Scopes.cpp
  ShadowAqlItemRow.cpp
  ShardLocking.cpp
  SharedAqlItemBlockPtr.cpp
  SharedQueryState.cpp
  ShortStringStorage.cpp
  SimpleModifier.cpp
  SingleRowFetcher.cpp
  SkipResult.cpp
  SortCondition.cpp
  SortElement.cpp
  SortInformation.cpp
  SortRegister.cpp
  SortedRowsStorageBackendMemory.cpp
  SortedRowsStorageBackendStaged.cpp
  Timing.cpp
  TraversalConditionFinder.cpp
  TraverserEngineShardLists.cpp
  UpdateReplaceModifier.cpp
  UpsertModifier.cpp
  VarUsageFinder.cpp
  Variable.cpp
  VariableGenerator.cpp
)

if (USE_V8)
  target_sources(arango_aql PRIVATE
    V8ErrorHandler.cpp)
endif()

# Generate the AQL lexer/parser into the build tree. These add_custom_command
# calls must live in the same directory scope as the consuming target
# (arango_aql, declared above) — when placed in arangod/Aql/Parser/CMakeLists.txt
# CMake's Ninja generator emits phantom build edges without a COMMAND, and the
# files are never actually generated.
add_custom_command(
  OUTPUT
    ${CMAKE_CURRENT_BINARY_DIR}/Parser/grammar.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/Parser/grammar.hpp
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/Parser
  COMMAND ${CMAKE_SOURCE_DIR}/utils/bison-c.sh
    ${BISON_EXECUTABLE}
    ${CMAKE_CURRENT_BINARY_DIR}/Parser/grammar.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Parser/grammar.y
  MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/Parser/grammar.y
  VERBATIM
)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/Parser/tokens.cpp
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/Parser
  COMMAND ${FLEX_EXECUTABLE} -L
    -o ${CMAKE_CURRENT_BINARY_DIR}/Parser/tokens.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Parser/tokens.ll
  MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/Parser/tokens.ll
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/Parser/grammar.hpp
  VERBATIM
)

add_subdirectory(ExecutionNode)
add_subdirectory(Executor)
add_subdirectory(Function)
add_subdirectory(Parser)
add_subdirectory(Optimizer)

target_link_libraries(arango_aql
  arango_geo
  arango_graph
  arango_indexes
  arango_iresearch
  arango_network
  immer)

target_include_directories(arango_aql PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_BINARY_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")
