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

#include "Graph/PathManagement/PathStoreTracer.cpp"
#include "Graph/PathManagement/PathStoreTracer.h"
#include "Graph/PathManagement/PathValidator.cpp"
#include "Graph/Providers/ProviderTracer.cpp"
#include "Graph/Queues/QueueTracer.cpp"
#include "Graph/PathManagement/PathValidatorTracer.cpp"

using namespace ::arangodb::graph;
using namespace ::arangodb::tests::graph;

template class ::arangodb::graph::PathResult<MockGraphProvider,
                                             MockGraphProvider::Step>;

template class ::arangodb::graph::SingleProviderPathResult<
    MockGraphProvider, PathStore<MockGraphProvider::Step>,
    MockGraphProvider::Step>;

template class ::arangodb::graph::SingleProviderPathResult<
    MockGraphProvider, PathStoreTracer<PathStore<MockGraphProvider::Step>>,
    MockGraphProvider::Step>;

template class ::arangodb::graph::ProviderTracer<MockGraphProvider>;

template class ::arangodb::graph::PathStore<MockGraphProvider::Step>;

template class ::arangodb::graph::PathValidator<
    MockGraphProvider, PathStore<MockGraphProvider::Step>,
    VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>;

template class ::arangodb::graph::PathValidator<
    ProviderTracer<MockGraphProvider>,
    PathStoreTracer<PathStore<MockGraphProvider::Step>>,
    VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>;

template class ::arangodb::graph::PathStoreTracer<
    PathStore<MockGraphProvider::Step>>;

template class ::arangodb::graph::PathValidator<
    MockGraphProvider, PathStoreTracer<PathStore<MockGraphProvider::Step>>,
    VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>;

template class ::arangodb::graph::TwoSidedEnumerator<
    FifoQueue<MockGraphProvider::Step>, PathStore<MockGraphProvider::Step>,
    MockGraphProvider,
    PathValidator<MockGraphProvider, PathStore<MockGraphProvider::Step>,
                  VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>>;

template class ::arangodb::graph::WeightedTwoSidedEnumerator<
    WeightedQueue<MockGraphProvider::Step>, PathStore<MockGraphProvider::Step>,
    MockGraphProvider,
    PathValidator<MockGraphProvider, PathStore<MockGraphProvider::Step>,
                  VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>>;

template class ::arangodb::graph::WeightedShortestPathEnumerator<
    WeightedQueue<MockGraphProvider::Step>, PathStore<MockGraphProvider::Step>,
    MockGraphProvider,
    PathValidator<MockGraphProvider, PathStore<MockGraphProvider::Step>,
                  VertexUniquenessLevel::NONE, EdgeUniquenessLevel::NONE>>;

template class arangodb::graph::PathValidatorTracer<
    arangodb::graph::PathValidator<
        arangodb::graph::ProviderTracer<MockGraphProvider>,
        arangodb::graph::PathStoreTracer<
            arangodb::graph::PathStore<MockGraphProvider::Step>>,
        (arangodb::graph::VertexUniquenessLevel)1,
        (arangodb::graph::EdgeUniquenessLevel)1>>;

// BFS with PATH uniqueness
template class ::arangodb::graph::OneSidedEnumerator<
    BFSConfiguration<MockGraphProvider, VertexUniquenessLevel::PATH,
                     EdgeUniquenessLevel::PATH, false>>;
template class ::arangodb::graph::OneSidedEnumerator<
    BFSConfiguration<MockGraphProvider, VertexUniquenessLevel::PATH,
                     EdgeUniquenessLevel::PATH, true>>;

// DFS with PATH uniqueness
template class ::arangodb::graph::OneSidedEnumerator<
    DFSConfiguration<MockGraphProvider, VertexUniquenessLevel::PATH,
                     EdgeUniquenessLevel::PATH, false>>;
template class ::arangodb::graph::OneSidedEnumerator<
    DFSConfiguration<MockGraphProvider, VertexUniquenessLevel::PATH,
                     EdgeUniquenessLevel::PATH, true>>;

template class ::arangodb::graph::QueueTracer<
    FifoQueue<MockGraphProvider::Step>>;

template class ::arangodb::graph::QueueTracer<
    LifoQueue<MockGraphProvider::Step>>;

template class ::arangodb::graph::QueueTracer<
    WeightedQueue<MockGraphProvider::Step>>;

template class ::arangodb::graph::TwoSidedEnumerator<
    QueueTracer<FifoQueue<MockGraphProvider::Step>>,
    PathStore<MockGraphProvider::Step>, MockGraphProvider,
    PathValidator<MockGraphProvider, PathStore<MockGraphProvider::Step>,
                  VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>>;

template class ::arangodb::graph::TwoSidedEnumerator<
    QueueTracer<FifoQueue<MockGraphProvider::Step>>,
    PathStoreTracer<PathStore<MockGraphProvider::Step>>, MockGraphProvider,
    PathValidator<
        MockGraphProvider,
        PathStoreTracer<::arangodb::graph::PathStore<MockGraphProvider::Step>>,
        VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>>;

template class ::arangodb::graph::WeightedTwoSidedEnumerator<
    QueueTracer<WeightedQueue<MockGraphProvider::Step>>,
    PathStore<MockGraphProvider::Step>, MockGraphProvider,
    PathValidator<MockGraphProvider, PathStore<MockGraphProvider::Step>,
                  VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>>;

template class ::arangodb::graph::WeightedTwoSidedEnumerator<
    QueueTracer<WeightedQueue<MockGraphProvider::Step>>,
    PathStoreTracer<PathStore<MockGraphProvider::Step>>, MockGraphProvider,
    PathValidator<
        MockGraphProvider,
        PathStoreTracer<::arangodb::graph::PathStore<MockGraphProvider::Step>>,
        VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>>;
