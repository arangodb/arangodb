////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "ShortestPathExecutor.h"

#include "Aql/OutputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/SingleRowFetcher.h"
#include "Basics/StaticStrings.h"
#include "Graph/algorithm-aliases.h"
#include "Graph/Providers/SingleServerProvider.h"
#include "Graph/Providers/ClusterProvider.h"
#include "Graph/Steps/SingleServerProviderStep.h"
#include "Graph/Steps/ClusterProviderStep.h"

#include "ShortestPathExecutor.tpp"

using SingleServer = SingleServerProvider<SingleServerProviderStep>;
using Cluster = ClusterProvider<ClusterProviderStep>;

// Infos
template class ::arangodb::aql::ShortestPathExecutorInfos<
    arangodb::graph::ShortestPathEnumerator<SingleServer>>;
template class ::arangodb::aql::ShortestPathExecutorInfos<
    TracedShortestPathEnumerator<SingleServer>>;
template class ::arangodb::aql::ShortestPathExecutorInfos<
    ShortestPathEnumerator<Cluster>>;
template class ::arangodb::aql::ShortestPathExecutorInfos<
    TracedShortestPathEnumerator<Cluster>>;

template class ::arangodb::aql::ShortestPathExecutorInfos<
    WeightedShortestPathEnumerator<SingleServer>>;
template class ::arangodb::aql::ShortestPathExecutorInfos<
    TracedWeightedShortestPathEnumerator<SingleServer>>;
template class ::arangodb::aql::ShortestPathExecutorInfos<
    WeightedShortestPathEnumerator<Cluster>>;
template class ::arangodb::aql::ShortestPathExecutorInfos<
    TracedWeightedShortestPathEnumerator<Cluster>>;

// Executor
template class ::arangodb::aql::ShortestPathExecutor<
    ShortestPathEnumerator<SingleServer>>;
template class ::arangodb::aql::ShortestPathExecutor<
    ShortestPathEnumerator<Cluster>>;
template class ::arangodb::aql::ShortestPathExecutor<
    TracedShortestPathEnumerator<SingleServer>>;
template class ::arangodb::aql::ShortestPathExecutor<
    TracedShortestPathEnumerator<Cluster>>;

template class ::arangodb::aql::ShortestPathExecutor<
    WeightedShortestPathEnumerator<SingleServer>>;
template class ::arangodb::aql::ShortestPathExecutor<
    WeightedShortestPathEnumerator<Cluster>>;
template class ::arangodb::aql::ShortestPathExecutor<
    TracedWeightedShortestPathEnumerator<SingleServer>>;
template class ::arangodb::aql::ShortestPathExecutor<
    TracedWeightedShortestPathEnumerator<Cluster>>;
