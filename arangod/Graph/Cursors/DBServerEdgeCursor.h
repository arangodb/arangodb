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
                               std::string_view vertex) {
  {t.all(callback)};
  { t.next(callback) } -> std::convertible_to<bool>;
  { t.rearm(vertex) };
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

 public:
  DBServerEdgeCursor(std::vector<T> cursors) : _cursors{std::move(cursors)} {}

  bool next(EdgeCursor::Callback const& callback) override {
    do {
      if (_currentCursor == _cursors.size()) {
        return false;
      }
      if (_cursors[_currentCursor].next(callback)) {
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

    for (auto& cursor : _cursors) {
      cursor.rearm(vertex);
    }
  }

  /** Gives number of HTTP requests performed.

      Always 0 on dbserver.
  */
  std::uint64_t httpRequests() const override { return 0; }
};

}  // namespace arangodb::graph
