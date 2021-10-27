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
/// @author Michael Hackstein
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Graph/TraverserOptions.h"

#include <memory>
#include <numeric>
#include <unordered_map>

namespace arangodb {

namespace velocypack {
class HashedStringRef;
class Builder;
}  // namespace velocypack

namespace aql {
class TraversalStats;
}

namespace graph {

class PathResultInterface {
 public:
  PathResultInterface() {}
  virtual ~PathResultInterface() {}

  virtual auto toVelocyPack(arangodb::velocypack::Builder& builder) -> void = 0;
};

class TraversalEnumerator {
 public:
  using VertexRef = arangodb::velocypack::HashedStringRef;
  TraversalEnumerator(){};
  virtual ~TraversalEnumerator() {}

  // NOTE: keepPathStore is only required for 3.8 compatibility and
  // can be removed in the version after 3.9
  virtual void clear(bool keepPathStore) = 0;
  [[nodiscard]] virtual bool isDone() const = 0;

  // NOTE: keepPathStore is only required for 3.8 compatibility and
  // can be removed in the version after 3.9
  virtual void reset(VertexRef source, size_t depth = 0, double weight = 0.0,
                     bool keepPathStore = false) = 0;
  virtual auto prepareIndexExpressions(aql::Ast* ast) -> void = 0;
  virtual auto getNextPath() -> std::unique_ptr<PathResultInterface> = 0;
  virtual bool skipPath() = 0;
  virtual auto destroyEngines() -> void = 0;

  virtual auto stealStats() -> aql::TraversalStats = 0;
};

}  // namespace graph
}  // namespace arangodb
