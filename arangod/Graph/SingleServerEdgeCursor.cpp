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
#include "StorageEngine/DocumentIdentifierToken.h"
#include "Transaction/Methods.h"
#include "Utils/OperationCursor.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

using namespace arangodb;
using namespace arangodb::graph;

////////////////////////////////////////////////////////////////////////////////
/// @brief Get a document by it's ID. Also lazy locks the collection.
///        If DOCUMENT_NOT_FOUND this function will return normally
///        with a OperationResult.failed() == true.
///        On all other cases this function throws.
////////////////////////////////////////////////////////////////////////////////

SingleServerEdgeCursor::SingleServerEdgeCursor(
    ManagedDocumentResult* mmdr, BaseOptions* opts, size_t nrCursors,
    std::vector<size_t> const* mapping)
    : _opts(opts),
      _trx(opts->trx()),
      _mmdr(mmdr),
      _cursors(),
      _currentCursor(0),
      _currentSubCursor(0),
      _cachePos(0),
      _internalCursorMapping(mapping) {
  TRI_ASSERT(_mmdr != nullptr);
  _cursors.reserve(nrCursors);
  _cache.reserve(1000);
  if (_opts->cache() == nullptr) {
    throw;
  }
};

SingleServerEdgeCursor::~SingleServerEdgeCursor() {
  for (auto& it : _cursors) {
    for (auto& it2 : it) {
      delete it2;
    }
  }
}

void SingleServerEdgeCursor::getDocAndRunCallback(OperationCursor* cursor, Callback callback) {
  auto collection = cursor->collection();
  auto etkn = std::make_unique<SingleServerEdgeDocumentToken>(collection->cid(), _cache[_cachePos++]);
  if (collection->readDocument(_trx, etkn->token(), *_mmdr)) {
    VPackSlice edgeDocument(_mmdr->vpack());
    _opts->cache()->increaseCounter();
    if (_internalCursorMapping != nullptr) {
      TRI_ASSERT(_currentCursor < _internalCursorMapping->size());
      callback(std::move(etkn), edgeDocument, _internalCursorMapping->at(_currentCursor));
    } else {
      callback(std::move(etkn), edgeDocument, _currentCursor);
    }
  }
}

bool SingleServerEdgeCursor::advanceCursor(OperationCursor*& cursor, std::vector<OperationCursor*>& cursorSet) {
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

bool SingleServerEdgeCursor::next(std::function<void(std::unique_ptr<EdgeDocumentToken>&&, VPackSlice, size_t)> callback) {
  // fills callback with next EdgeDocumentToken and Slice that contains the ohter side of the edge
  // (we are standing on a node and want to iterate all connected edges

  if (_currentCursor == _cursors.size()) {
    return false;
  }

  // There is still something in the cache
  if (_cachePos < _cache.size()) {
    //get the collection
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
      if(cursor->hasExtra()){
        bool operationSuccessfull = false;
        auto extraCB = [&](DocumentIdentifierToken const& token, VPackSlice edge){
          if (token._data != 0) {
            operationSuccessfull = true;
            auto etkn = std::make_unique<SingleServerEdgeDocumentToken>(cursor->collection()->cid(), token);
            if (_internalCursorMapping != nullptr) {
              TRI_ASSERT(_currentCursor < _internalCursorMapping->size());
              callback(std::move(etkn), edge, _internalCursorMapping->at(_currentCursor));
            } else {
              callback(std::move(etkn), edge, _currentCursor);
            }
          }
        };
        cursor->nextWithExtra(extraCB,1);
        if (operationSuccessfull) {
          return true;
        }
      } else {
        _cache.clear();
        auto cb = [&](DocumentIdentifierToken const& token) {
          if (token._data != 0) {
            // Document not found
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

void SingleServerEdgeCursor::readAll(
    std::function<void(std::unique_ptr<EdgeDocumentToken>&&, VPackSlice, size_t)> callback) {
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
      auto cid = collection->cid();
      if (cursor->hasExtra()) {
        auto cb = [&](DocumentIdentifierToken const& token, VPackSlice edge) {
          callback(std::make_unique<SingleServerEdgeDocumentToken>(cid, token), edge, cursorId);
        };
        cursor->allWithExtra(cb);
      } else {
        auto cb = [&](DocumentIdentifierToken const& token) {
          if (collection->readDocument(_trx, token, *_mmdr)) {
            _opts->cache()->increaseCounter();
            VPackSlice doc(_mmdr->vpack());
            callback(std::make_unique<SingleServerEdgeDocumentToken>(cid, token), doc, cursorId);
          }
        };
        cursor->all(cb);
      }
    }
  }
}

