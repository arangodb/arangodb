////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_GRAPH_PATH_STORE_H
#define ARANGOD_GRAPH_PATH_STORE_H 1

#include <queue>

#include "Basics/ResourceUsage.h"
#include "Basics/debugging.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

using namespace arangodb;

namespace arangodb {

namespace aql {
struct AqlValue;
}

namespace graph {

template <class ProviderType, class Step>
class PathResult;

/*
 * Schreier element:
 * {
 *  vertex: "<reference>",
 *  inboundEdge: "<reference>",
 *  previous: "<size_t"> // Index entry of prev. vertex
 * }
 */

template <class StepType>
class PathStore {
 public:
  using Step = StepType;

 public:
  explicit PathStore(arangodb::ResourceMonitor& resourceMonitor);
  ~PathStore();

  /// @brief schreier vector to store the visited vertices
  std::vector<Step> _schreier;

  arangodb::ResourceMonitor& _resourceMonitor;

  // @brief Method to verify whether path is needed
  bool testPath(Step);

  // @brief reset
  void reset();

  // @brief Method adds a new element to the schreier vector
  // returns the index of inserted element
  size_t append(Step step);

  // @brief returns the current vector size
  size_t size() const { return _schreier.size(); }

  template <class ProviderType>
  void buildPath(Step const& vertex, PathResult<ProviderType, Step>& path) const;

  template <class ProviderType>
  void reverseBuildPath(Step const& vertex, PathResult<ProviderType, Step>& path) const;

  // TODO: to be defined - section idea: methods to convenient build paths for AQL
  arangodb::aql::AqlValue lastVertexToAqlValue();
  arangodb::aql::AqlValue lastEdgeToAqlValue();
  arangodb::aql::AqlValue pathToAqlValue(arangodb::velocypack::Builder& result);
};

}  // namespace graph
}  // namespace arangodb

#endif  // ARANGOD_GRAPH_QUEUE_H
