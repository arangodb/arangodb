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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Containers/FlatHashMap.h"
#include "Graph/PathManagement/PathResult.h"
#include "Graph/PathManagement/PathValidatorOptions.h"

#include "velocypack/HashedStringRef.h"

namespace arangodb::graph::dijkstra {

template<class QueueType, class PathStoreType, class ProviderType,
         class PathValidatorType>
class Dijkstra {
  using VertexRef = velocypack::HashedStringRef;
  using Step = typename ProviderType::Step;

 public:
  Dijkstra(ProviderType&& provider, PathValidatorOptions validatorOptions,
           arangodb::ResourceMonitor& resourceMonitor);
  ~Dijkstra();
  auto clear() -> void;
  auto reset(VertexRef center) -> void;
  [[nodiscard]] auto noPathLeft() const -> bool;
  [[nodiscard]] auto peekQueue() const -> Step const&;
  [[nodiscard]] auto isQueueEmpty() const -> bool;
  [[nodiscard]] auto doneWithDepth() const -> bool;

  auto buildPath(Step const& vertexInShell,
                 PathResult<ProviderType, Step>& path) -> void;

  [[nodiscard]] auto matchResultsInShell(
      Step const& match, PathValidatorType const& otherSideValidator) -> double;

  // @brief returns a positive double in a match has been found.
  // returns -1.0 if no match has been found.
  [[nodiscard]] auto computeNeighbourhoodOfNextVertex() -> double;

  [[nodiscard]] auto hasBeenVisited(Step const& step) -> bool;

  [[nodiscard]] auto getRadius() const noexcept -> double { return _radius; }

  auto provider() -> ProviderType&;

 private:
  auto clearProvider() -> void;

 private:
  // TODO: Double check if we really need the monitor here. Currently unused.
  arangodb::ResourceMonitor& _resourceMonitor;

  // This stores all paths processed by this ball
  PathStoreType _interior;

  double _radius = 0.0;

  // The next elements to process
  QueueType _queue;

  ProviderType _provider;

  PathValidatorType _validator;
  containers::FlatHashMap<typename Step::VertexType, std::vector<size_t>>
      _visitedNodes;
};

}  // namespace arangodb::graph::dijkstra
