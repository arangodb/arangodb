#include <Aql/ExecutionBlockImpl/ExecutionBlockImpl.tpp>

// Avoid compiling everything again in the tests
#ifndef ARANGODB_INCLUDED_FROM_GTESTS

template class ::arangodb::aql::ExecutionBlockImpl<
    CalculationExecutor<CalculationType::Condition>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    CalculationExecutor<CalculationType::Reference>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    CalculationExecutor<CalculationType::V8Condition>>;
template class ::arangodb::aql::ExecutionBlockImpl<ConstrainedSortExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<CountCollectExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<DistinctCollectExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<EnumerateCollectionExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<EnumerateListExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<FilterExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<HashedCollectExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<AccuWindowExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<WindowExecutor>;

template class ::arangodb::aql::ExecutionBlockImpl<IdExecutor<ConstFetcher>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    IdExecutor<SingleRowFetcher<BlockPassthrough::Enable>>>;
template class ::arangodb::aql::ExecutionBlockImpl<IndexExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<LimitExecutor>;

template class ::arangodb::aql::ExecutionBlockImpl<NoResultsExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<ReturnExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<ShortestPathExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<
    EnumeratePathsExecutor<arangodb::graph::KShortestPathsFinder>>;

/* SingleServer */
template class ::arangodb::aql::ExecutionBlockImpl<
    EnumeratePathsExecutor<KPathRefactored>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    EnumeratePathsExecutor<KPathRefactoredTracer>>;

/* Cluster */
template class ::arangodb::aql::ExecutionBlockImpl<
    EnumeratePathsExecutor<KPathRefactoredCluster>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    EnumeratePathsExecutor<KPathRefactoredClusterTracer>>;

template class ::arangodb::aql::ExecutionBlockImpl<SortedCollectExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<SortExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<SubqueryEndExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<SubqueryStartExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<TraversalExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<SortingGatherExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<
    ParallelUnsortedGatherExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<UnsortedGatherExecutor>;

template class ::arangodb::aql::ExecutionBlockImpl<ModificationExecutor<
    SingleRowFetcher<BlockPassthrough::Disable>, InsertModifier>>;
template class ::arangodb::aql::ExecutionBlockImpl<ModificationExecutor<
    SingleRowFetcher<BlockPassthrough::Disable>, RemoveModifier>>;
template class ::arangodb::aql::ExecutionBlockImpl<ModificationExecutor<
    SingleRowFetcher<BlockPassthrough::Disable>, UpdateReplaceModifier>>;
template class ::arangodb::aql::ExecutionBlockImpl<ModificationExecutor<
    SingleRowFetcher<BlockPassthrough::Disable>, UpsertModifier>>;

template class ::arangodb::aql::ExecutionBlockImpl<
    MaterializeExecutor<RegisterId>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    MaterializeExecutor<std::string const &>>;

#ifdef USE_ENTERPRISE
template class ::arangodb::aql::ExecutionBlockImpl<
    ::arangodb::iresearch::OffsetMaterializeExecutor>;
#endif

#endif
