////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
#include <unordered_set>

#include "Basics/ResourceUsage.h"
#include "Basics/debugging.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb {

namespace aql {
struct AqlValue;
}

namespace graph {

template <class ProviderType, class Step>
class PathResult;

class ValidationResult;

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

  // @brief reset
  void reset();

  // @brief Method adds a new element to the schreier vector
  // returns the index of inserted element
  size_t append(Step step);

  // @brief returns the current vector size
  size_t size() const { return _schreier.size(); }

  auto visitReversePath(Step const& step,
                        std::function<bool(Step const&)> const& visitor) const -> bool;

  template <class ProviderType>
  auto buildPath(Step const& vertex, PathResult<ProviderType, Step>& path) const -> void;

  template <class ProviderType>
  auto reverseBuildPath(Step const& vertex, PathResult<ProviderType, Step>& path) const -> void;

 private:
  /// @brief schreier vector to store the visited vertices
  std::vector<Step> _schreier;

  arangodb::ResourceMonitor& _resourceMonitor;
};

}  // namespace graph
}  // namespace arangodb

#endif  // ARANGOD_GRAPH_QUEUE_H
