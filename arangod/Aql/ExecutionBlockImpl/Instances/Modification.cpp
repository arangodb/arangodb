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

#include <Aql/InsertModifier.h>
#include <Aql/RemoveModifier.h>
#include <Aql/UpdateReplaceModifier.h>
#include <Aql/UpsertModifier.h>

#include <Aql/ExecutionBlockImpl/ExecutionBlockImpl.tpp>

template class ::arangodb::aql::ExecutionBlockImpl<ModificationExecutor<
    SingleRowFetcher<BlockPassthrough::Disable>, InsertModifier>>;
template class ::arangodb::aql::ExecutionBlockImpl<ModificationExecutor<
    SingleRowFetcher<BlockPassthrough::Disable>, RemoveModifier>>;
template class ::arangodb::aql::ExecutionBlockImpl<ModificationExecutor<
    SingleRowFetcher<BlockPassthrough::Disable>, UpdateReplaceModifier>>;
template class ::arangodb::aql::ExecutionBlockImpl<ModificationExecutor<
    SingleRowFetcher<BlockPassthrough::Disable>, UpsertModifier>>;

// IndexTag, Insert, Remove, Update,Replace, Upsert are only tags for this one
template class ::arangodb::aql::ExecutionBlockImpl<
    SingleRemoteModificationExecutor<IndexTag>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    SingleRemoteModificationExecutor<Insert>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    SingleRemoteModificationExecutor<Remove>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    SingleRemoteModificationExecutor<Update>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    SingleRemoteModificationExecutor<Replace>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    SingleRemoteModificationExecutor<Upsert>>;
