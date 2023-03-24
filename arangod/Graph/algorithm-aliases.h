////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
#include "Graph/Enumerators/WeightedTwoSidedEnumerator.h"

#include "Graph/Queues/FifoQueue.h"
#include "Graph/Queues/LifoQueue.h"
#include "Graph/Queues/QueueTracer.h"
#include "Graph/Queues/WeightedQueue.h"

#include "Graph/PathManagement/PathStore.h"
#include "Graph/PathManagement/PathStoreTracer.h"
#include "Graph/PathManagement/PathValidator.h"
#include "Graph/PathManagement/PathValidatorTracer.h"
#include "Graph/Providers/ProviderTracer.h"
#include "Graph/Types/UniquenessLevel.h"

namespace arangodb::graph {

template<class Provider>
using TwoSidedEnumeratorWithProvider = TwoSidedEnumerator<
    FifoQueue<typename Provider::Step>, PathStore<typename Provider::Step>,
    Provider,
    PathValidator<Provider, PathStore<typename Provider::Step>,
                  VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>>;

template<class Provider>
using TwoSidedEnumeratorWithProviderWeighted = WeightedTwoSidedEnumerator<
    WeightedQueue<typename Provider::Step>, PathStore<typename Provider::Step>,
    Provider,
    PathValidator<Provider, PathStore<typename Provider::Step>,
                  VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>>;

template<class Provider>
using TracedTwoSidedEnumeratorWithProvider = TwoSidedEnumerator<
    QueueTracer<FifoQueue<typename Provider::Step>>,
    PathStoreTracer<PathStore<typename Provider::Step>>,
    ProviderTracer<Provider>,
    PathValidator<ProviderTracer<Provider>,
                  PathStoreTracer<PathStore<typename Provider::Step>>,
                  VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>>;

template<class Provider>
using TracedTwoSidedEnumeratorWithProviderWeighted = WeightedTwoSidedEnumerator<
    QueueTracer<WeightedQueue<typename Provider::Step>>,
    PathStoreTracer<PathStore<typename Provider::Step>>,
    ProviderTracer<Provider>,
    PathValidator<ProviderTracer<Provider>,
                  PathStoreTracer<PathStore<typename Provider::Step>>,
                  VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>>;

// K_PATH implementation
template<class Provider>
using KPathEnumerator = TwoSidedEnumeratorWithProvider<Provider>;

// K_PATH implementation using Tracing
template<class Provider>
using TracedKPathEnumerator = TracedTwoSidedEnumeratorWithProvider<Provider>;

// ALL_SHORTEST_PATHS implementation
template<class Provider>
using AllShortestPathsEnumerator = TwoSidedEnumeratorWithProvider<Provider>;

// K_SHORTEST_PATHS implementation
template<class Provider>
using KShortestPathsEnumerator = TwoSidedEnumeratorWithProvider<Provider>;

// K_SHORTEST_PATHS implementation using Tracing
template<class Provider>
using TracedKShortestPathsEnumerator =
    TracedTwoSidedEnumeratorWithProvider<Provider>;

// WEIGHTED_K_SHORTEST_PATHS implementation
template<class Provider>
using WeightedKShortestPathsEnumerator =
    TwoSidedEnumeratorWithProviderWeighted<Provider>;

// WEIGHTED_K_SHORTEST_PATHS implementation using Tracing
template<class Provider>
using TracedWeightedKShortestPathsEnumerator =
    TracedTwoSidedEnumeratorWithProviderWeighted<Provider>;

// ALL_SHORTEST_PATHS implementation using Tracing
template<class Provider>
using TracedAllShortestPathsEnumerator =
    TracedTwoSidedEnumeratorWithProvider<Provider>;

// SHORTEST_PATH implementation
template<class Provider>
using ShortestPathEnumerator = TwoSidedEnumeratorWithProvider<Provider>;

template<class Provider>
using WeightedShortestPathEnumerator =
    TwoSidedEnumeratorWithProviderWeighted<Provider>;

// SHORTEST_PATH implementation using Tracing
template<class Provider>
using TracedShortestPathEnumerator =
    TracedTwoSidedEnumeratorWithProvider<Provider>;

template<class Provider>
using TracedWeightedShortestPathEnumerator =
    TracedTwoSidedEnumeratorWithProviderWeighted<Provider>;

template<class ProviderType, VertexUniquenessLevel vertexUniqueness,
         EdgeUniquenessLevel edgeUniqueness, bool useTracing>
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
  using Validator = typename std::conditional<
      useTracing,
      PathValidatorTracer<
          PathValidator<Provider, Store, vertexUniqueness, edgeUniqueness>>,
      PathValidator<Provider, Store, vertexUniqueness, edgeUniqueness>>::type;
};

template<class ProviderType, VertexUniquenessLevel vertexUniqueness,
         EdgeUniquenessLevel edgeUniqueness, bool useTracing>
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
  using Validator = typename std::conditional<
      useTracing,
      PathValidatorTracer<
          PathValidator<Provider, Store, vertexUniqueness, edgeUniqueness>>,
      PathValidator<Provider, Store, vertexUniqueness, edgeUniqueness>>::type;
};

template<class ProviderType, VertexUniquenessLevel vertexUniqueness,
         EdgeUniquenessLevel edgeUniqueness, bool useTracing>
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
  using Validator = typename std::conditional<
      useTracing,
      PathValidatorTracer<
          PathValidator<Provider, Store, vertexUniqueness, edgeUniqueness>>,
      PathValidator<Provider, Store, vertexUniqueness, edgeUniqueness>>::type;
};

// BFS Traversal Enumerator implementation
template<class Provider, VertexUniquenessLevel vertexUniqueness,
         EdgeUniquenessLevel edgeUniqueness>
using BFSEnumerator = OneSidedEnumerator<
    BFSConfiguration<Provider, vertexUniqueness, edgeUniqueness, false>>;

// BFS Traversal Enumerator implementation using Tracing
template<class Provider, VertexUniquenessLevel vertexUniqueness,
         EdgeUniquenessLevel edgeUniqueness>
using TracedBFSEnumerator = OneSidedEnumerator<
    BFSConfiguration<Provider, vertexUniqueness, edgeUniqueness, true>>;

// DFS Traversal Enumerator implementation
template<class Provider, VertexUniquenessLevel vertexUniqueness,
         EdgeUniquenessLevel edgeUniqueness>
using DFSEnumerator = OneSidedEnumerator<
    DFSConfiguration<Provider, vertexUniqueness, edgeUniqueness, false>>;

// DFS Traversal Enumerator implementation using Tracing
template<class Provider, VertexUniquenessLevel vertexUniqueness,
         EdgeUniquenessLevel edgeUniqueness>
using TracedDFSEnumerator = OneSidedEnumerator<
    DFSConfiguration<Provider, vertexUniqueness, edgeUniqueness, true>>;

// Weighted Traversal Enumerator implementation
// TODO: Needs to be renamed as soon as we replace the existing variant, which
// occupies this name
template<class Provider, VertexUniquenessLevel vertexUniqueness,
         EdgeUniquenessLevel edgeUniqueness>
using WeightedEnumeratorRefactored = OneSidedEnumerator<
    WeightedConfiguration<Provider, vertexUniqueness, edgeUniqueness, false>>;

// BFS Traversal Enumerator implementation using Tracing
template<class Provider, VertexUniquenessLevel vertexUniqueness,
         EdgeUniquenessLevel edgeUniqueness>
using TracedWeightedEnumerator = OneSidedEnumerator<
    WeightedConfiguration<Provider, vertexUniqueness, edgeUniqueness, true>>;

}  // namespace arangodb::graph
