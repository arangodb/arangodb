#include <Aql/ExecutionBlockImpl/ExecutionBlockImpl.tpp>

// Avoid compiling everything again in the tests
#ifndef ARANGODB_INCLUDED_FROM_GTESTS

template class ::arangodb::aql::ExecutionBlockImpl<
    IResearchViewHeapSortExecutor<aql::ExecutionTraits<
        false, false, false,
        arangodb::iresearch::MaterializeType::NotMaterialize>>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    IResearchViewHeapSortExecutor<aql::ExecutionTraits<
        false, false, false,
        arangodb::iresearch::MaterializeType::LateMaterialize>>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    IResearchViewHeapSortExecutor<aql::ExecutionTraits<
        false, false, false,
        arangodb::iresearch::MaterializeType::Materialize>>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    IResearchViewHeapSortExecutor<aql::ExecutionTraits<
        false, false, false,
        arangodb::iresearch::MaterializeType::NotMaterialize |
            arangodb::iresearch::MaterializeType::UseStoredValues>>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    IResearchViewHeapSortExecutor<aql::ExecutionTraits<
        false, false, false,
        arangodb::iresearch::MaterializeType::LateMaterialize |
            arangodb::iresearch::MaterializeType::UseStoredValues>>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    IResearchViewHeapSortExecutor<aql::ExecutionTraits<
        true, false, false,
        arangodb::iresearch::MaterializeType::NotMaterialize |
            arangodb::iresearch::MaterializeType::UseStoredValues>>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    IResearchViewHeapSortExecutor<aql::ExecutionTraits<
        true, false, false,
        arangodb::iresearch::MaterializeType::LateMaterialize |
            arangodb::iresearch::MaterializeType::UseStoredValues>>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    IResearchViewHeapSortExecutor<aql::ExecutionTraits<
        false, true, false,
        arangodb::iresearch::MaterializeType::NotMaterialize>>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    IResearchViewHeapSortExecutor<aql::ExecutionTraits<
        false, true, false,
        arangodb::iresearch::MaterializeType::LateMaterialize>>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    IResearchViewHeapSortExecutor<aql::ExecutionTraits<
        false, true, false,
        arangodb::iresearch::MaterializeType::Materialize>>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    IResearchViewHeapSortExecutor<aql::ExecutionTraits<
        false, true, false,
        arangodb::iresearch::MaterializeType::NotMaterialize |
            arangodb::iresearch::MaterializeType::UseStoredValues>>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    IResearchViewHeapSortExecutor<aql::ExecutionTraits<
        false, true, false,
        arangodb::iresearch::MaterializeType::LateMaterialize |
            arangodb::iresearch::MaterializeType::UseStoredValues>>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    IResearchViewHeapSortExecutor<aql::ExecutionTraits<
        true, true, false,
        arangodb::iresearch::MaterializeType::NotMaterialize |
            arangodb::iresearch::MaterializeType::UseStoredValues>>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    IResearchViewHeapSortExecutor<aql::ExecutionTraits<
        true, true, false,
        arangodb::iresearch::MaterializeType::LateMaterialize |
            arangodb::iresearch::MaterializeType::UseStoredValues>>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    IResearchViewHeapSortExecutor<aql::ExecutionTraits<
        false, false, true,
        arangodb::iresearch::MaterializeType::NotMaterialize>>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    IResearchViewHeapSortExecutor<aql::ExecutionTraits<
        false, false, true,
        arangodb::iresearch::MaterializeType::LateMaterialize>>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    IResearchViewHeapSortExecutor<aql::ExecutionTraits<
        false, false, true,
        arangodb::iresearch::MaterializeType::Materialize>>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    IResearchViewHeapSortExecutor<aql::ExecutionTraits<
        false, false, true,
        arangodb::iresearch::MaterializeType::NotMaterialize |
            arangodb::iresearch::MaterializeType::UseStoredValues>>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    IResearchViewHeapSortExecutor<aql::ExecutionTraits<
        false, false, true,
        arangodb::iresearch::MaterializeType::LateMaterialize |
            arangodb::iresearch::MaterializeType::UseStoredValues>>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    IResearchViewHeapSortExecutor<aql::ExecutionTraits<
        true, false, true,
        arangodb::iresearch::MaterializeType::NotMaterialize |
            arangodb::iresearch::MaterializeType::UseStoredValues>>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    IResearchViewHeapSortExecutor<aql::ExecutionTraits<
        true, false, true,
        arangodb::iresearch::MaterializeType::LateMaterialize |
            arangodb::iresearch::MaterializeType::UseStoredValues>>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    IResearchViewHeapSortExecutor<aql::ExecutionTraits<
        false, true, true,
        arangodb::iresearch::MaterializeType::NotMaterialize>>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    IResearchViewHeapSortExecutor<aql::ExecutionTraits<
        false, true, true,
        arangodb::iresearch::MaterializeType::LateMaterialize>>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    IResearchViewHeapSortExecutor<aql::ExecutionTraits<
        false, true, true, arangodb::iresearch::MaterializeType::Materialize>>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    IResearchViewHeapSortExecutor<aql::ExecutionTraits<
        false, true, true,
        arangodb::iresearch::MaterializeType::NotMaterialize |
            arangodb::iresearch::MaterializeType::UseStoredValues>>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    IResearchViewHeapSortExecutor<aql::ExecutionTraits<
        false, true, true,
        arangodb::iresearch::MaterializeType::LateMaterialize |
            arangodb::iresearch::MaterializeType::UseStoredValues>>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    IResearchViewHeapSortExecutor<aql::ExecutionTraits<
        true, true, true,
        arangodb::iresearch::MaterializeType::NotMaterialize |
            arangodb::iresearch::MaterializeType::UseStoredValues>>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    IResearchViewHeapSortExecutor<aql::ExecutionTraits<
        true, true, true,
        arangodb::iresearch::MaterializeType::LateMaterialize |
            arangodb::iresearch::MaterializeType::UseStoredValues>>>;

#endif
