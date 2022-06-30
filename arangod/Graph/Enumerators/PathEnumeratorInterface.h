////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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

#include <numeric>

namespace arangodb {
namespace velocypack {
class Builder;
class HashedStringRef;
}
namespace aql {
class TraversalStats;
class QueryContext;
}

namespace graph {

struct TwoSidedEnumeratorOptions;
class PathValidatorOptions;

class PathResultInterface;

class PathEnumeratorInterface {
 public:
  enum PathEnumeratorType {
    K_PATH,
    K_SHORTEST_PATH,
    SHORTEST_PATH
  };

  template<class ProviderName>
  static auto createEnumerator(
      aql::QueryContext& query,
      typename ProviderName::Options&& forwardProviderOptions,
      typename ProviderName::Options&& backwardProviderOptions,
      TwoSidedEnumeratorOptions enumeratorOptions,
      PathValidatorOptions validatorOptions, PathEnumeratorType type,
      bool useTracing) -> std::unique_ptr<PathEnumeratorInterface>;

  PathEnumeratorInterface() = default;
  virtual ~PathEnumeratorInterface() = default;

  using VertexRef = arangodb::velocypack::HashedStringRef;

  /**
   * @brief clear local data storage and discard all prepared results
   */
  virtual auto clear() -> void = 0;

  /**
   * @brief Quick test if the finder can prove there is no more data available.
   *        It can respond with false, even though there is no path left.
   * @return true There will be no further path.
   * @return false There is a chance that there is more data available.
   */
  [[nodiscard]] virtual auto isDone() const -> bool = 0;

  /**
   * @brief Reset to new source and target vertices.
   * This API uses string references, this class will not take responsibility
   * for the referenced data. It is caller's responsibility to retain the
   * underlying data and make sure the strings stay valid until next
   * call of reset.
   *
   * @param source The source vertex to start the paths
   * @param target The target vertex to end the paths
   */
  virtual auto reset(VertexRef source, VertexRef target, size_t depth = 0) -> void = 0;

  /**
   * @brief Get the next path, if available written into the result build.
   * The given builder will be not be cleared, this function requires a
   * prepared builder to write into.
   *
   * @param result Input and output, this needs to be an open builder,
   * where the path can be placed into.
   * Can be empty, or an openArray, or the value of an object.
   * Guarantee: Every returned path matches the conditions handed in via
   * options. No path is returned twice, it is intended that paths overlap.
   * @return true Found and written a path, result is modified.
   * @return false No path found, result has not been changed.
   */
  virtual auto getNextPath(arangodb::velocypack::Builder& result) -> bool = 0;
  virtual auto getNextPath_New() -> arangodb::graph::PathResultInterface* = 0;

  /**
   * @brief Skip the next Path, like getNextPath, but does not return the path.
   *
   * @return true Found and skipped a path.
   * @return false No path found.
   */

  virtual auto skipPath() -> bool = 0;
  virtual auto destroyEngines() -> void = 0;

  /**
   * @brief Return statistics generated since
   * the last time this method was called.
   */
  virtual auto stealStats() -> aql::TraversalStats = 0;

};

}
}
