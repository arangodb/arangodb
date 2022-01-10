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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Graph/Enumerators/OneSidedEnumerator.h"
#include "Graph/Enumerators/TwoSidedEnumerator.h"

#include "Graph/Queues/FifoQueue.h"
#include "Graph/Queues/LifoQueue.h"
#include "Graph/Queues/QueueTracer.h"
#include "Graph/Queues/WeightedQueue.h"

#include "Graph/PathManagement/PathStore.h"
#include "Graph/PathManagement/PathStoreTracer.h"
#include "Graph/PathManagement/PathValidator.h"
#include "Graph/Providers/ProviderTracer.h"
#include "Graph/Types/UniquenessLevel.h"

namespace arangodb {
namespace graph {

// K_PATH implementation
template<class Provider>
using KPathEnumerator = TwoSidedEnumerator<
    FifoQueue<typename Provider::Step>, PathStore<typename Provider::Step>,
    Provider,
    PathValidator<Provider, PathStore<typename Provider::Step>,
                  VertexUniquenessLevel::PATH>>;

// K_PATH implementation using Tracing
template<class Provider>
using TracedKPathEnumerator = TwoSidedEnumerator<
    QueueTracer<FifoQueue<typename Provider::Step>>,
    PathStoreTracer<PathStore<typename Provider::Step>>,
    ProviderTracer<Provider>,
    PathValidator<ProviderTracer<Provider>,
                  PathStoreTracer<PathStore<typename Provider::Step>>,
                  VertexUniquenessLevel::PATH>>;

template<class ProviderType, VertexUniquenessLevel vertexUniqueness,
         bool useTracing>
struct BFSConfiguration {
  using Provider =
      typename std::conditional<useTracing, ProviderTracer<ProviderType>,
                                ProviderType>::type;
  using Step = typename Provider::Step;
  using Queue =
      typename std::conditional<useTracing, QueueTracer<FifoQueue<Step>>,
                                FifoQueue<Step>>::type;
  using Store =
      typename std::conditional<useTracing, PathStoreTracer<PathStore<Step>>,
                                PathStore<Step>>::type;
  using Validator = PathValidator<Provider, Store, vertexUniqueness>;
};

template<class ProviderType, VertexUniquenessLevel vertexUniqueness,
         bool useTracing>
struct DFSConfiguration {
  using Provider =
      typename std::conditional<useTracing, ProviderTracer<ProviderType>,
                                ProviderType>::type;
  using Step = typename Provider::Step;
  using Queue =
      typename std::conditional<useTracing, QueueTracer<LifoQueue<Step>>,
                                LifoQueue<Step>>::type;
  using Store =
      typename std::conditional<useTracing, PathStoreTracer<PathStore<Step>>,
                                PathStore<Step>>::type;
  using Validator = PathValidator<Provider, Store, vertexUniqueness>;
};

template<class ProviderType, VertexUniquenessLevel vertexUniqueness,
         bool useTracing>
struct WeightedConfiguration {
  using Provider =
      typename std::conditional<useTracing, ProviderTracer<ProviderType>,
                                ProviderType>::type;
  using Step = typename Provider::Step;
  using Queue =
      typename std::conditional<useTracing, QueueTracer<WeightedQueue<Step>>,
                                WeightedQueue<Step>>::type;
  using Store =
      typename std::conditional<useTracing, PathStoreTracer<PathStore<Step>>,
                                PathStore<Step>>::type;
  using Validator = PathValidator<Provider, Store, vertexUniqueness>;
};

// BFS Traversal Enumerator implementation
template<class Provider, VertexUniquenessLevel vertexUniqueness>
using BFSEnumerator =
    OneSidedEnumerator<BFSConfiguration<Provider, vertexUniqueness, false>>;

// BFS Traversal Enumerator implementation using Tracing
template<class Provider, VertexUniquenessLevel vertexUniqueness>
using TracedBFSEnumerator =
    OneSidedEnumerator<BFSConfiguration<Provider, vertexUniqueness, true>>;

// DFS Traversal Enumerator implementation
template<class Provider, VertexUniquenessLevel vertexUniqueness>
using DFSEnumerator =
    OneSidedEnumerator<DFSConfiguration<Provider, vertexUniqueness, false>>;

// DFS Traversal Enumerator implementation using Tracing
template<class Provider, VertexUniquenessLevel vertexUniqueness>
using TracedDFSEnumerator =
    OneSidedEnumerator<DFSConfiguration<Provider, vertexUniqueness, true>>;

// Weighted Traversal Enumerator implementation
// TODO: Needs to be renamed as soon as we replace the existing variant, whic
// occupies this name
template<class Provider, VertexUniquenessLevel vertexUniqueness>
using WeightedEnumeratorRefactored = OneSidedEnumerator<
    WeightedConfiguration<Provider, vertexUniqueness, false>>;

// BFS Traversal Enumerator implementation using Tracing
template<class Provider, VertexUniquenessLevel vertexUniqueness>
using TracedWeightedEnumerator =
    OneSidedEnumerator<WeightedConfiguration<Provider, vertexUniqueness, true>>;

}  // namespace graph
}  // namespace arangodb
