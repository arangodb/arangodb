////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "SingleServerEdgeCursor.h"

#include "Graph/BaseOptions.h"
#include "Graph/EdgeDocumentToken.h"
#include "Graph/TraverserCache.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "Utils/OperationCursor.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;
using namespace arangodb::graph;

////////////////////////////////////////////////////////////////////////////////
/// @brief Get a document by it's ID. Also lazy locks the collection.
///        If DOCUMENT_NOT_FOUND this function will return normally
///        with a OperationResult.failed() == true.
///        On all other cases this function throws.
////////////////////////////////////////////////////////////////////////////////

SingleServerEdgeCursor::SingleServerEdgeCursor(BaseOptions* opts, size_t nrCursors,
                                               std::vector<size_t> const* mapping)
    : _opts(opts),
      _trx(opts->trx()),
      _currentCursor(0),
      _currentSubCursor(0),
      _cachePos(0),
      _internalCursorMapping(mapping) {
  _cursors.reserve(nrCursors);
  _cache.reserve(1000);
  if (_opts->cache() == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "no cache present for single server edge cursor");
  }
}

SingleServerEdgeCursor::~SingleServerEdgeCursor() {
  for (auto& it : _cursors) {
    for (auto& it2 : it) {
      delete it2;
    }
  }
}

#ifdef USE_ENTERPRISE
static bool CheckInaccesible(transaction::Methods* trx, VPackSlice const& edge) {
  // for skipInaccessibleCollections we need to check the edge
  // document, in that case nextWithExtra has no benefit
  TRI_ASSERT(edge.isString());
  arangodb::velocypack::StringRef str(edge);
  size_t pos = str.find('/');
  TRI_ASSERT(pos != std::string::npos);
  return trx->isInaccessibleCollection(str.substr(0, pos).toString());
}
#endif

void SingleServerEdgeCursor::getDocAndRunCallback(OperationCursor* cursor, EdgeCursor::Callback const& callback) {
  auto collection = cursor->collection();
  if (collection == nullptr) {
    return;
  }
  EdgeDocumentToken etkn(collection->id(), _cache[_cachePos++]);
  collection->readDocumentWithCallback(
      _trx, etkn.localDocumentId(), [&](LocalDocumentId const&, VPackSlice edgeDoc) {
#ifdef USE_ENTERPRISE
        if (_trx->state()->options().skipInaccessibleCollections) {
          // TODO: we only need to check one of these
          VPackSlice from = transaction::helpers::extractFromFromDocument(edgeDoc);
          VPackSlice to = transaction::helpers::extractToFromDocument(edgeDoc);
          if (CheckInaccesible(_trx, from) || CheckInaccesible(_trx, to)) {
            return;
          }
        }
#endif
        _opts->cache()->increaseCounter();
        if (_internalCursorMapping != nullptr) {
          TRI_ASSERT(_currentCursor < _internalCursorMapping->size());
          callback(std::move(etkn), edgeDoc, _internalCursorMapping->at(_currentCursor));
        } else {
          callback(std::move(etkn), edgeDoc, _currentCursor);
        }
      });
}

bool SingleServerEdgeCursor::advanceCursor(OperationCursor*& cursor,
                                           std::vector<OperationCursor*>& cursorSet) {
  ++_currentSubCursor;
  if (_currentSubCursor >= cursorSet.size()) {
    ++_currentCursor;
    _currentSubCursor = 0;
    if (_currentCursor == _cursors.size()) {
      // We are done, all cursors exhausted.
      return false;
    }
    cursorSet = _cursors[_currentCursor];
  }

  cursor = cursorSet[_currentSubCursor];
  // If we switch the cursor. We have to clear the cache.
  _cache.clear();
  return true;
}

bool SingleServerEdgeCursor::next(EdgeCursor::Callback const& callback) {
  // fills callback with next EdgeDocumentToken and Slice that contains the
  // ohter side of the edge (we are standing on a node and want to iterate all
  // connected edges

  if (_currentCursor == _cursors.size()) {
    return false;
  }

  // There is still something in the cache
  if (_cachePos < _cache.size()) {
    // get the collection
    auto cur = _cursors[_currentCursor][_currentSubCursor];
    getDocAndRunCallback(cur, callback);
    return true;
  }

  // We need to refill the cache.
  _cachePos = 0;
  auto cursorSet = _cursors[_currentCursor];

  // get current cursor
  auto cursor = cursorSet[_currentSubCursor];

  // NOTE: We cannot clear the cache,
  // because the cursor expect's it to be filled.
  do {
    if (cursorSet.empty() || !cursor->hasMore()) {
      if (!advanceCursor(cursor, cursorSet)) {
        return false;
      }
    } else {
      if (cursor->hasExtra()) {
        bool operationSuccessful = false;
        auto extraCB = [&](LocalDocumentId const& token, VPackSlice edge) {
          if (token.isSet()) {
#ifdef USE_ENTERPRISE
            if (_trx->state()->options().skipInaccessibleCollections &&
                CheckInaccesible(_trx, edge)) {
              return;
            }
#endif
            if (cursor->collection() == nullptr) {
              return;
            }
            operationSuccessful = true;
            auto etkn = EdgeDocumentToken(cursor->collection()->id(), token);
            if (_internalCursorMapping != nullptr) {
              TRI_ASSERT(_currentCursor < _internalCursorMapping->size());
              callback(std::move(etkn), edge, _internalCursorMapping->at(_currentCursor));
            } else {
              callback(std::move(etkn), edge, _currentCursor);
            }
          }
        };
        cursor->nextWithExtra(extraCB, 1);
        if (operationSuccessful) {
          return true;
        }
      } else {
        _cache.clear();
        auto cb = [&](LocalDocumentId const& token) {
          if (token.isSet()) {
            // Document found
            _cache.emplace_back(token);
          }
        };
        bool tmp = cursor->next(cb, 1000);
        TRI_ASSERT(tmp == cursor->hasMore());
      }
    }
  } while (_cache.empty());
  TRI_ASSERT(!_cache.empty());
  TRI_ASSERT(_cachePos < _cache.size());
  getDocAndRunCallback(cursor, callback);
  return true;
}

void SingleServerEdgeCursor::readAll(EdgeCursor::Callback const& callback) {
  size_t cursorId = 0;
  for (_currentCursor = 0; _currentCursor < _cursors.size(); ++_currentCursor) {
    if (_internalCursorMapping != nullptr) {
      TRI_ASSERT(_currentCursor < _internalCursorMapping->size());
      cursorId = _internalCursorMapping->at(_currentCursor);
    } else {
      cursorId = _currentCursor;
    }
    auto& cursorSet = _cursors[_currentCursor];
    for (auto& cursor : cursorSet) {
      LogicalCollection* collection = cursor->collection();
      if (collection == nullptr) {
        continue;
      }
      auto cid = collection->id();
      if (cursor->hasExtra()) {
        auto cb = [&](LocalDocumentId const& token, VPackSlice edge) {
#ifdef USE_ENTERPRISE
          if (_trx->state()->options().skipInaccessibleCollections &&
              CheckInaccesible(_trx, edge)) {
            return;
          }
#endif
          callback(EdgeDocumentToken(cid, token), edge, cursorId);
        };
        cursor->allWithExtra(cb);
      } else {
        auto cb = [&](LocalDocumentId const& token) {
          collection->readDocumentWithCallback(_trx, token, [&](LocalDocumentId const&, VPackSlice edgeDoc) {
#ifdef USE_ENTERPRISE
            if (_trx->state()->options().skipInaccessibleCollections) {
              // TODO: we only need to check one of these
              VPackSlice from = transaction::helpers::extractFromFromDocument(edgeDoc);
              VPackSlice to = transaction::helpers::extractToFromDocument(edgeDoc);
              if (CheckInaccesible(_trx, from) || CheckInaccesible(_trx, to)) {
                return;
              }
            }
#endif
            _opts->cache()->increaseCounter();
            callback(EdgeDocumentToken(cid, token), edgeDoc, cursorId);
          });
        };
        cursor->all(cb);
      }
    }
  }
}
