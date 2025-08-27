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
template<IndexCursor T>
class DBServerEdgeCursor final : public EdgeCursor {
 public:
 private:
  // per collection there can be several index cursors
  std::vector<std::vector<T>> _cursors;
  size_t _currentCursor = 0;
  size_t _currentSubCursor = 0;

 public:
  DBServerEdgeCursor(std::vector<std::vector<T>> cursors)
      : _cursors{std::move(cursors)} {}

  bool next(EdgeCursor::Callback const& callback) override {
    if (_currentCursor == _cursors.size()) {
      return false;
    }

    TRI_ASSERT(_cursors[_currentCursor].size() > _currentSubCursor);

    do {
      if (_cursors[_currentCursor].empty()) {
        if (!advanceCursor()) {
          return false;
        }
      } else {
        if (_cursors[_currentCursor][_currentSubCursor].next(callback)) {
          return true;
        } else {
          if (!advanceCursor()) {
            return false;
          }
        }
      }
    } while (true);
  }

  void readAll(EdgeCursor::Callback const& callback) override {
    for (_currentCursor = 0; _currentCursor < _cursors.size();
         ++_currentCursor) {
      auto& cursorSet = _cursors[_currentCursor];
      for (auto& cursor : cursorSet) {
        cursor.all(callback);
      }
    }
  }

  void rearm(std::string_view vertex, uint64_t depth) override {
    _currentCursor = 0;
    _currentSubCursor = 0;

    for (auto& collectionCursors : _cursors) {
      for (auto& indexCursor : collectionCursors) {
        indexCursor.rearm(vertex);
      }
    }
  }

  /// @brief number of HTTP requests performed. always 0 in single server
  std::uint64_t httpRequests() const override { return 0; }

 private:
  // returns false if cursor can not be further advanced
  bool advanceCursor() {
    TRI_ASSERT(!_cursors.empty());
    ++_currentSubCursor;
    if (_currentSubCursor >= _cursors[_currentCursor].size()) {
      ++_currentCursor;
      _currentSubCursor = 0;
      if (_currentCursor == _cursors.size()) {
        // We are done, all cursors exhausted.
        return false;
      }
    }
    // If we switch the cursor. We have to clear the cache.
    return true;
  }
};

}  // namespace arangodb::graph
