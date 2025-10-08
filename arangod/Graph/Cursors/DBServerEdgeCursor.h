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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string_view>
#include <vector>

#include "Graph/Cursors/EdgeCursor.h"

namespace arangodb::graph {

template<typename T>
concept IndexCursor = requires(T t, EdgeCursor::Callback const& callback,
                               std::string_view vertex, uint64_t batchSize) {
  {t.all(callback)};
  { t.next(callback) } -> std::convertible_to<bool>;
  { t.nextBatch(callback, batchSize) } -> std::same_as<uint64_t>;
  { t.rearm(vertex) };
  { t.hasMore() } -> std::convertible_to<const bool>;
};

/**
   On a DBServer, iterates over all edges of a given vertex that are part of the
   given input cursors.

   Input cursors are all relevant index cursors on all involved collections /
   shards.
 */
template<IndexCursor T>
class DBServerEdgeCursor final : public EdgeCursor {
 private:
  std::vector<T> _cursors;
  size_t _currentCursor = 0;
  std::optional<std::string_view> _currentVertex;
  std::optional<uint64_t> _depth;

 public:
  DBServerEdgeCursor(std::vector<T> cursors,
                     std::optional<uint64_t> depth = std::nullopt)
      : _cursors{std::move(cursors)}, _depth{depth} {}

  bool next(EdgeCursor::Callback const& callback) override {
    return nextBatch(callback, 1);
  }

  bool nextBatch(EdgeCursor::Callback const& callback,
                 uint64_t batchSize) override {
    uint64_t requiredItems = batchSize;
    do {
      if (_currentCursor == _cursors.size()) {
        return false;
      }
      requiredItems -=
          _cursors[_currentCursor].nextBatch(callback, requiredItems);
      if (requiredItems == 0) {
        return true;
      }
      _currentCursor++;
    } while (true);
  }

  void readAll(EdgeCursor::Callback const& callback) override {
    for (_currentCursor = 0; _currentCursor < _cursors.size();
         ++_currentCursor) {
      _cursors[_currentCursor].all(callback);
    }
  }

  /**
     Resets cursor to another vertex.

     Depth input is ignored here.
   */
  void rearm(std::string_view vertex, uint64_t /*depth*/) override {
    _currentCursor = 0;
    _currentVertex = vertex;

    for (auto& cursor : _cursors) {
      cursor.rearm(vertex);
    }
  }

  bool hasMore() const override {
    if (_currentCursor >= _cursors.size()) {
      return false;
    }

    return _cursors[_currentCursor].hasMore() ||
           (_currentCursor + 1 < _cursors.size() &&
            _cursors[_currentCursor + 1].hasMore());
  }

  std::optional<std::string_view> currentVertex() const override {
    return _currentVertex;
  }
  std::optional<uint64_t> currentDepth() const override { return {_depth}; }

  /** Gives number of HTTP requests performed.

      Always 0 on dbserver.
  */
  std::uint64_t httpRequests() const override { return 0; }
};

}  // namespace arangodb::graph
