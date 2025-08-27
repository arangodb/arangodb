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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "Graph/BaseOptions.h"
#include "Graph/Cursors/EdgeCursor.h"
#include "Graph/TraverserCache.h"
#include "Indexes/IndexIterator.h"
#include "VocBase/Identifiers/DataSourceId.h"

namespace arangodb {
class LocalDocumentId;
class LogicalCollection;
struct ResourceMonitor;

namespace aql {
struct Variable;
}

namespace transaction {
class Methods;
}

namespace velocypack {
class Slice;
}

namespace graph {
struct BaseOptions;

struct CollectionIndexCursor {
  void all(EdgeCursor::Callback const& callback);
  bool next(EdgeCursor::Callback const& callback);
  void rearm(std::string_view vertex);

  CollectionIndexCursor(transaction::Methods::IndexHandle idxHandle,
                        size_t cursorId, uint16_t coveringIndexPosition,
                        aql::AstNode* indexCondition,
                        std::optional<size_t> conditionMemberToUpdate,
                        transaction::Methods* trx,
                        TraverserCache* traverserCache,
                        aql::Variable const* tmpVar, ResourceMonitor& monitor)
      : _idxHandle{idxHandle},
        _cursorId{cursorId},
        _coveringIndexPosition{coveringIndexPosition},
        _indexCondition{indexCondition},
        _conditionMemberToUpdate{conditionMemberToUpdate},
        _trx{trx},
        _traverserCache{traverserCache},
        _tmpVar{tmpVar},
        _monitor{monitor} {
    TRI_ASSERT(_traverserCache != nullptr);
    _cache.reserve(1000);
  }

 private:
  void prepareIndexCondition(std::string_view vertex);
  std::function<bool(LocalDocumentId, IndexIteratorCoveringData&)>
  coveringCallback(bool& operationSuccessful, DataSourceId const& sourceId,
                   size_t cursorId, uint16_t coveringPosition,
                   EdgeCursor::Callback const& callback);
  std::function<bool(LocalDocumentId, aql::DocumentData&&, VPackSlice)>
  nonCoveringCallback(DataSourceId const& sourceId, size_t cursorId,
                      EdgeCursor::Callback const& callback);

  std::unique_ptr<IndexIterator> _cursor;
  transaction::Methods::IndexHandle _idxHandle;  // needed for rearming
  DataSourceId _collectionId;
  size_t _cursorId;

  uint16_t _coveringIndexPosition;
  aql::AstNode* _indexCondition;  // for all cursors in on collection
  std::optional<size_t> _conditionMemberToUpdate;  // "

  transaction::Methods* _trx;
  TraverserCache* _traverserCache;
  aql::Variable const* _tmpVar;  // only needed in ctor and rearm
  ResourceMonitor& _monitor;     // only needed in ctor and rearm

  std::vector<LocalDocumentId> _cache;
  size_t _cachePos = 0;
};

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

auto createCollectionIndexCursors(
    std::vector<BaseOptions::LookupInfo> const& lookupInfos,
    aql::Variable const* tmpVar, transaction::Methods* trx,
    TraverserCache* traverserCache, ResourceMonitor& monitor)
    -> std::vector<std::vector<CollectionIndexCursor>>;

}  // namespace graph
}  // namespace arangodb
