////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020-2020 ArangoDB GmbH, Cologne, Germany
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

#include "TwoSidedEnumerator.h"

#include "Basics/system-compiler.h"

#include "Graph/PathManagement/PathResult.h"

#include <velocypack/Builder.h>
#include <velocypack/HashedStringRef.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::graph;

template <class QueueType, class PathStoreType, class ProviderType>
TwoSidedEnumerator<QueueType, PathStoreType, ProviderType>::TwoSidedEnumerator(ProviderType&& provider)
    : _provider(std::move(provider)), _left{Direction::FORWARD}, _right{Direction::BACKWARD}, _resultPath{} {}

template <class QueueType, class PathStoreType, class ProviderType>
TwoSidedEnumerator<QueueType, PathStoreType, ProviderType>::~TwoSidedEnumerator() {}

template <class QueueType, class PathStoreType, class ProviderType>
void TwoSidedEnumerator<QueueType, PathStoreType, ProviderType>::clear() {
  // Cases:
  // - Needs to clear Queue?
  // - Needs to clear Store?
  // - Needs to clear Provider?
  // - Needs to clear itself?
}

/**
 * @brief Quick test if the finder can prove there is no more data available.
 *        It can respond with false, even though there is no path left.
 * @return true There will be no further path.
 * @return false There is a chance that there is more data available.
 */
template <class QueueType, class PathStoreType, class ProviderType>
bool TwoSidedEnumerator<QueueType, PathStoreType, ProviderType>::isDone() const {
  // Cases:
  // - Provider does not have more data
  // - PathStore does not return / save any additional Paths
  // - Queue does not contain any looseEnds anymore (?)
  // - OR: Just local algorithm check?
  return true;
}

/**
 * @brief Reset to new source and target vertices.
 * This API uses string references, this class will not take responsibility
 * for the referenced data. It is caller's responsibility to retain the
 * underlying data and make sure the StringRefs stay valid until next
 * call of reset.
 *
 * @param source The source vertex to start the paths
 * @param target The target vertex to end the paths
 */
template <class QueueType, class PathStoreType, class ProviderType>
void TwoSidedEnumerator<QueueType, PathStoreType, ProviderType>::reset(VertexRef source,
                                                                       VertexRef target) {}

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
template <class QueueType, class PathStoreType, class ProviderType>
bool TwoSidedEnumerator<QueueType, PathStoreType, ProviderType>::getNextPath(
    arangodb::velocypack::Builder& result) {
  while (!isDone()) {
    while (_results.empty() && !searchDone()) {
      if (_searchLeft) {
        if (ADB_UNLIKELY(_left.doneWithDepth())) {
          startNextDepth();
        } else {
          _left.computeNeighbourhoodOfNextVertex(_right, _results);
        }
      } else {
        if (ADB_UNLIKELY(_right.doneWithDepth())) {
          startNextDepth();
        } else {
          _right.computeNeighbourhoodOfNextVertex(_left, _results);
        }
      }
    }

    while (!_results.empty()) {
      auto [leftVertex, rightVertex] = _results.back();

      _resultPath.clear();
      _left.buildPath(leftVertex, _resultPath);
      _right.buildPath(rightVertex, _resultPath);

      // result done
      _results.pop_back();
      if (_resultPath.isValid()) {
        _resultPath.toVelocyPack(result);
        return true;
      }
    }
  }
  return false;
}

/**
 * @brief Skip the next Path, like getNextPath, but does not return the path.
 *
 * @return true Found and skipped a path.
 * @return false No path found.
 */

template <class QueueType, class PathStoreType, class ProviderType>
bool TwoSidedEnumerator<QueueType, PathStoreType, ProviderType>::skipPath() {
  return false;
}

template <class QueueType, class PathStoreType, class ProviderType>
auto TwoSidedEnumerator<QueueType, PathStoreType, ProviderType>::searchDone() const
    -> bool {
  return _left.noPathLeft() || _right.noPathLeft();
  // TODO: include getDepth
}