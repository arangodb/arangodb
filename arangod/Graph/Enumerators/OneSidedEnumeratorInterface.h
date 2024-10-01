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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Graph/TraverserOptions.h"

#include <memory>
#include <numeric>
#include <vector>

namespace arangodb {

namespace velocypack {
class HashedStringRef;
class Builder;
}  // namespace velocypack

namespace aql {
class TraversalStats;
}

namespace graph {
struct VertexDescription;
struct OneSidedEnumeratorOptions;
class PathValidatorOptions;

#ifdef USE_ENTERPRISE
namespace enterprise {
template<class Provider>
struct SmartGraphResponse;
}
#endif

class PathResultInterface {
 public:
  PathResultInterface() {}
  virtual ~PathResultInterface() = default;

  virtual auto toVelocyPack(velocypack::Builder& builder) -> void = 0;
  virtual auto lastVertexToVelocyPack(velocypack::Builder& builder) -> void = 0;
  virtual auto lastEdgeToVelocyPack(velocypack::Builder& builder) -> void = 0;
};

class TraversalEnumerator {
 public:
  template<class Provider>
  static auto createEnumerator(
      traverser::TraverserOptions::Order order,
      traverser::TraverserOptions::UniquenessLevel uniqueVertices,
      traverser::TraverserOptions::UniquenessLevel uniqueEdges,
      aql::QueryContext& query,
      typename Provider::Options&& baseProviderOptions,
      graph::PathValidatorOptions&& pathValidatorOptions,
      graph::OneSidedEnumeratorOptions&& enumeratorOptions, bool useTracing)
      -> std::unique_ptr<TraversalEnumerator>;

  using VertexRef = velocypack::HashedStringRef;
  TraversalEnumerator() {}
  virtual ~TraversalEnumerator() {}

  virtual void clear(bool keepPathStore) = 0;
  [[nodiscard]] virtual bool isDone() const = 0;

  virtual void reset(VertexRef source, size_t depth = 0, double weight = 0.0,
                     bool keepPathStore = false) = 0;

  virtual void resetManyStartVertices(
      std::vector<VertexDescription> const& vertices) = 0;

  virtual auto prepareIndexExpressions(aql::Ast* ast) -> void = 0;
  virtual auto getNextPath() -> std::unique_ptr<PathResultInterface> = 0;
#ifdef USE_ENTERPRISE
  virtual auto smartSearch(size_t amountOfExpansions,
                           velocypack::Builder& result) -> void = 0;
#endif
  virtual bool skipPath() = 0;
  virtual auto destroyEngines() -> void = 0;

  virtual auto stealStats() -> aql::TraversalStats = 0;

  virtual auto validatorUsesPrune() const -> bool = 0;
  virtual auto validatorUsesPostFilter() const -> bool = 0;
  virtual auto setValidatorContext(aql::InputAqlItemRow& inputRow) -> void = 0;
  virtual auto unprepareValidatorContext() -> void = 0;
};

}  // namespace graph
}  // namespace arangodb
