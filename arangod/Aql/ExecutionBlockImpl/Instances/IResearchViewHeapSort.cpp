////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include <Aql/ExecutionBlockImpl/ExecutionBlockImpl.tpp>

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
