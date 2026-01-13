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
/// @author Heiko Kernbach
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "./MockGraphProvider.h"
#include "Graph/Enumerators/OneSidedEnumerator.cpp"
#include "Graph/Enumerators/TwoSidedEnumerator.cpp"
#include "Graph/Enumerators/WeightedTwoSidedEnumerator.cpp"
#include "Graph/Enumerators/WeightedShortestPathEnumerator.cpp"
#include "Graph/PathManagement/PathResult.cpp"
#include "Graph/PathManagement/PathStore.cpp"
#include "Graph/PathManagement/SingleProviderPathResult.cpp"
#include "Graph/Queues/FifoQueue.h"
#include "Graph/Queues/LifoQueue.h"
#include "Graph/Queues/WeightedQueue.h"
#include "Graph/PathManagement/PathValidator.cpp"

using namespace ::arangodb::graph;
using namespace ::arangodb::tests::graph;

template class ::arangodb::graph::PathResult<MockGraphProvider,
                                             MockGraphProvider::Step>;

template class ::arangodb::graph::SingleProviderPathResult<
    MockGraphProvider, PathStore<MockGraphProvider::Step>,
    MockGraphProvider::Step>;

template class ::arangodb::graph::PathStore<MockGraphProvider::Step>;

template class ::arangodb::graph::PathValidator<
    MockGraphProvider, PathStore<MockGraphProvider::Step>,
    VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>;

template class ::arangodb::graph::TwoSidedEnumerator<
    MockGraphProvider,
    PathValidator<MockGraphProvider, PathStore<MockGraphProvider::Step>,
                  VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>>;

template class ::arangodb::graph::WeightedTwoSidedEnumerator<MockGraphProvider>;

template class ::arangodb::graph::WeightedShortestPathEnumerator<
    MockGraphProvider>;

// BFS with PATH uniqueness
template class ::arangodb::graph::OneSidedEnumerator<BFSConfiguration<
    MockGraphProvider, VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>>;

// DFS with PATH uniqueness
template class ::arangodb::graph::OneSidedEnumerator<DFSConfiguration<
    MockGraphProvider, VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>>;
