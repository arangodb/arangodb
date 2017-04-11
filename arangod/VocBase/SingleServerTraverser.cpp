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

#include "SingleServerTraverser.h"
#include "Basics/StringRef.h"

#include "Aql/AqlValue.h"
#include "Graph/BreadthFirstEnumerator.h"
#include "Graph/NeighborsEnumerator.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/TraverserCache.h"

using namespace arangodb;
using namespace arangodb::traverser;
using namespace arangodb::graph;

////////////////////////////////////////////////////////////////////////////////
/// @brief Get a document by it's ID. Also lazy locks the collection.
///        If DOCUMENT_NOT_FOUND this function will return normally
///        with a OperationResult.failed() == true.
///        On all other cases this function throws.
////////////////////////////////////////////////////////////////////////////////

SingleServerEdgeCursor::SingleServerEdgeCursor(ManagedDocumentResult* mmdr,
    TraverserOptions* opts,
    size_t nrCursors, std::vector<size_t> const* mapping)
    : _opts(opts),
      _trx(opts->_trx),
      _mmdr(mmdr), 
      _cursors(),
      _currentCursor(0),
      _currentSubCursor(0),
      _cachePos(0),
      _internalCursorMapping(mapping) {
  TRI_ASSERT(_mmdr != nullptr);
  _cursors.reserve(nrCursors);
  _cache.reserve(1000);
};

bool SingleServerEdgeCursor::next(std::function<void(StringRef const&, VPackSlice, size_t)> callback) {
  if (_currentCursor == _cursors.size()) {
    return false;
  }
  if (_cachePos < _cache.size()) {
    LogicalCollection* collection = _cursors[_currentCursor][_currentSubCursor]->collection();
    if (collection->readDocument(_trx, _cache[_cachePos++], *_mmdr)) {
      VPackSlice edgeDocument(_mmdr->vpack());
      std::string eid = _trx->extractIdString(edgeDocument);
      StringRef persId = _opts->cache()->persistString(StringRef(eid));
      if (_internalCursorMapping != nullptr) {
        TRI_ASSERT(_currentCursor < _internalCursorMapping->size());
        callback(persId, edgeDocument, _internalCursorMapping->at(_currentCursor));
      } else {
        callback(persId, edgeDocument, _currentCursor);
      }
    }
    
    return true;
  }
  // We need to refill the cache.
  _cachePos = 0;
  auto cursorSet = _cursors[_currentCursor];
  while (cursorSet.empty()) {
    // Fast Forward to the next non-empty cursor set
    _currentCursor++;
    _currentSubCursor = 0;
    if (_currentCursor == _cursors.size()) {
      return false;
    }
    cursorSet = _cursors[_currentCursor];
  }
  auto cursor = cursorSet[_currentSubCursor];
  // NOTE: We cannot clear the cache,
  // because the cursor expect's it to be filled.
  do {
    if (!cursor->hasMore()) {
      // This one is exhausted, next
      ++_currentSubCursor;
      while (_currentSubCursor == cursorSet.size()) {
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
    } else {
      _cache.clear();
      auto cb = [&] (DocumentIdentifierToken const& token) {
        _cache.emplace_back(token);
      };
      bool tmp = cursor->getMore(cb, 1000);
      TRI_ASSERT(tmp == cursor->hasMore());
    }
  } while (_cache.empty());

  TRI_ASSERT(_cachePos < _cache.size());
  LogicalCollection* collection = cursor->collection();
  if (collection->readDocument(_trx, _cache[_cachePos++], *_mmdr)) {
    VPackSlice edgeDocument(_mmdr->vpack());
    std::string eid = _trx->extractIdString(edgeDocument);
    StringRef persId = _opts->cache()->persistString(StringRef(eid));
    if (_internalCursorMapping != nullptr) {
      TRI_ASSERT(_currentCursor < _internalCursorMapping->size());
      callback(persId, edgeDocument, _internalCursorMapping->at(_currentCursor));
    } else {
      callback(persId, edgeDocument, _currentCursor);
    }
  }
  return true;
}

void SingleServerEdgeCursor::readAll(std::function<void(StringRef const&, arangodb::velocypack::Slice, size_t&)> callback) {
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
      auto cb = [&] (DocumentIdentifierToken const& token) {
        if (collection->readDocument(_trx, token, *_mmdr)) {
          VPackSlice doc(_mmdr->vpack());
          std::string tmpId = _trx->extractIdString(doc);
          StringRef edgeId = _opts->cache()->persistString(StringRef(tmpId)); 
          callback(edgeId, doc, cursorId);
        }
      };
      while (cursor->getMore(cb, 1000)) {
      }
    }
  }
}

SingleServerTraverser::SingleServerTraverser(TraverserOptions* opts,
                                             transaction::Methods* trx,
                                             ManagedDocumentResult* mmdr)
  : Traverser(opts, trx, mmdr) {}

SingleServerTraverser::~SingleServerTraverser() {}

aql::AqlValue SingleServerTraverser::fetchVertexData(StringRef vid) {
  return _opts->cache()->fetchAqlResult(vid);
}

aql::AqlValue SingleServerTraverser::fetchEdgeData(StringRef edge) {  
  return _opts->cache()->fetchAqlResult(edge);
}

void SingleServerTraverser::addVertexToVelocyPack(StringRef vid,
                                                  VPackBuilder& result) {
  _opts->cache()->insertIntoResult(vid, result);
}

void SingleServerTraverser::addEdgeToVelocyPack(StringRef edge,
    VPackBuilder& result) {
  _opts->cache()->insertIntoResult(edge, result);
}

void SingleServerTraverser::setStartVertex(std::string const& vid) {
  _startIdBuilder->clear();
  _startIdBuilder->add(VPackValue(vid));
  VPackSlice idSlice = _startIdBuilder->slice();

  if (!vertexMatchesConditions(idSlice, 0)) {
    // Start vertex invalid
    _done = true;
    return;
  }

  StringRef persId = _opts->cache()->persistString(StringRef(vid));
  _vertexGetter->reset(persId);

  if (_opts->useBreadthFirst) {
    if (_canUseOptimizedNeighbors) {
      _enumerator.reset(new NeighborsEnumerator(this, idSlice, _opts));
    } else {
      _enumerator.reset(new BreadthFirstEnumerator(this, idSlice, _opts));
    }
  } else {
    _enumerator.reset(new DepthFirstEnumerator(this, vid, _opts));
  }
  _done = false;
}

size_t SingleServerTraverser::getAndResetReadDocuments() {
  return _opts->cache()->getAndResetInsertedDocuments();
}

bool SingleServerTraverser::getVertex(VPackSlice edge,
                                      std::vector<StringRef>& result) {
  return _vertexGetter->getVertex(edge, result);
}

bool SingleServerTraverser::getSingleVertex(VPackSlice edge, StringRef const sourceVertexId,
                                            uint64_t depth, StringRef& targetVertexId) {
  return _vertexGetter->getSingleVertex(edge, sourceVertexId, depth, targetVertexId);
}
