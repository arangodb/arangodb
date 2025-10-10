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

#pragma once

#include "Graph/Enumerators/OneSidedEnumerator.h"
#include "Graph/Enumerators/TwoSidedEnumerator.h"
#include "Graph/Enumerators/WeightedTwoSidedEnumerator.h"
#include "Graph/Enumerators/WeightedShortestPathEnumerator.h"
#include "Graph/Enumerators/YenEnumerator.h"

#include "Graph/Queues/BatchedLifoQueue.h"
#include "Graph/Queues/FifoQueue.h"
#include "Graph/Queues/LifoQueue.h"
#include "Graph/Queues/WeightedQueue.h"

#include "Graph/PathManagement/PathStore.h"
#include "Graph/PathManagement/PathValidator.h"
#include "Graph/PathManagement/PathValidatorTabooWrapper.h"
#include "Graph/Types/UniquenessLevel.h"

namespace arangodb::graph {

template<class Provider>
using TwoSidedEnumeratorWithProvider = TwoSidedEnumerator<
    Provider,
    PathValidator<Provider, PathStore<typename Provider::Step>,
                  VertexUniquenessLevel::PATH, EdgeUniquenessLevel::PATH>>;

template<class Provider>
using TwoSidedEnumeratorWithProviderWeighted =
    WeightedTwoSidedEnumerator<Provider>;

// SHORTEST_PATH implementation
template<class Provider>
using ShortestPathEnumerator = TwoSidedEnumerator<
    Provider,
    PathValidator<Provider, PathStore<typename Provider::Step>,
                  VertexUniquenessLevel::GLOBAL, EdgeUniquenessLevel::PATH>>;

template<class Provider>
using WeightedShortestPathEnumeratorAlias =
    WeightedShortestPathEnumerator<Provider>;

// SHORTEST_PATH for Yen:
template<class Provider>
using ShortestPathEnumeratorForYen = TwoSidedEnumerator<
    Provider, PathValidatorTabooWrapper<PathValidator<
                  Provider, PathStore<typename Provider::Step>,
                  VertexUniquenessLevel::GLOBAL, EdgeUniquenessLevel::PATH>>>;

// K_PATH implementation
template<class Provider>
using KPathEnumerator = TwoSidedEnumeratorWithProvider<Provider>;

// ALL_SHORTEST_PATHS implementation
template<class Provider>
using AllShortestPathsEnumerator = TwoSidedEnumeratorWithProvider<Provider>;

// K_SHORTEST_PATHS implementation
template<class Provider>
using KShortestPathsEnumerator = TwoSidedEnumeratorWithProvider<Provider>;

// Yen's algorithm implementation:
template<class Provider>
using YenEnumeratorWithProvider =
    YenEnumerator<Provider, ShortestPathEnumeratorForYen<Provider>,
                  false /* IsWeighted */>;

// Yen's algorithm implementation with weights:
template<class Provider>
using WeightedYenEnumeratorWithProvider =
    YenEnumerator<Provider, WeightedShortestPathEnumeratorAlias<Provider>,
                  true /* IsWeighted */>;

// WEIGHTED_K_SHORTEST_PATHS implementation
template<class Provider>
using WeightedKShortestPathsEnumerator =
    TwoSidedEnumeratorWithProviderWeighted<Provider>;

template<class ProviderType, VertexUniquenessLevel vertexUniqueness,
         EdgeUniquenessLevel edgeUniqueness>
struct BFSConfiguration {
  using Provider = ProviderType;
  using Step = typename Provider::Step;
  using Queue = FifoQueue<Step>;
  using Store = PathStore<Step>;
  using Validator =
      PathValidator<Provider, Store, vertexUniqueness, edgeUniqueness>;
};

template<class ProviderType, VertexUniquenessLevel vertexUniqueness,
         EdgeUniquenessLevel edgeUniqueness>
struct DFSConfiguration {
  using Provider = ProviderType;
  using Step = typename Provider::Step;
  using Queue = BatchedLifoQueue<Step>;
  using Store = PathStore<Step>;
  using Validator =
      PathValidator<Provider, Store, vertexUniqueness, edgeUniqueness>;
};

template<class ProviderType, VertexUniquenessLevel vertexUniqueness,
         EdgeUniquenessLevel edgeUniqueness>
struct WeightedConfiguration {
  using Provider = ProviderType;
  using Step = typename Provider::Step;
  using Queue = WeightedQueue<Step>;
  using Store = PathStore<Step>;
  using Validator =
      PathValidator<Provider, Store, vertexUniqueness, edgeUniqueness>;
};

// BFS Traversal Enumerator implementation
template<class Provider, VertexUniquenessLevel vertexUniqueness,
         EdgeUniquenessLevel edgeUniqueness>
using BFSEnumerator = OneSidedEnumerator<
    BFSConfiguration<Provider, vertexUniqueness, edgeUniqueness>>;

// DFS Traversal Enumerator implementation
template<class Provider, VertexUniquenessLevel vertexUniqueness,
         EdgeUniquenessLevel edgeUniqueness>
using DFSEnumerator = OneSidedEnumerator<
    DFSConfiguration<Provider, vertexUniqueness, edgeUniqueness>>;

// Weighted Traversal Enumerator implementation
// TODO: Needs to be renamed as soon as we replace the existing variant, which
// occupies this name
template<class Provider, VertexUniquenessLevel vertexUniqueness,
         EdgeUniquenessLevel edgeUniqueness>
using WeightedEnumerator = OneSidedEnumerator<
    WeightedConfiguration<Provider, vertexUniqueness, edgeUniqueness>>;

}  // namespace arangodb::graph
