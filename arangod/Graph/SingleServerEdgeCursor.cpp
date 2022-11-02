////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#include "Aql/AttributeNamePath.h"
#include "Aql/Projections.h"
#include "Aql/AstNode.h"
#include "Basics/StaticStrings.h"
#include "Graph/BaseOptions.h"
#include "Graph/EdgeDocumentToken.h"
#include "Graph/TraverserCache.h"
#include "Indexes/IndexIterator.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;
using namespace arangodb::graph;

namespace {
void PrepareIndexCondition(BaseOptions::LookupInfo const& info,
                           std::string_view vertex) {
  auto& node = info.indexCondition;
  TRI_ASSERT(node->numMembers() > 0);
  if (info.conditionNeedUpdate) {
    // We have to inject _from/_to iff the condition needs it
    auto dirCmp = node->getMemberUnchecked(info.conditionMemberToUpdate);
    TRI_ASSERT(dirCmp->type == aql::NODE_TYPE_OPERATOR_BINARY_EQ);
    TRI_ASSERT(dirCmp->numMembers() == 2);

    auto idNode = dirCmp->getMemberUnchecked(1);
    TRI_ASSERT(idNode->type == aql::NODE_TYPE_VALUE);
    TRI_ASSERT(idNode->isValueType(aql::VALUE_TYPE_STRING));
    // must edit node in place; TODO replace node?
    TEMPORARILY_UNLOCK_NODE(idNode);
    idNode->setStringValue(vertex.data(), vertex.length());
  }
}
}  // namespace

////////////////////////////////////////////////////////////////////////////////
/// @brief Get a document by it's ID. Also lazy locks the collection.
///        If DOCUMENT_NOT_FOUND this function will return normally
///        with a OperationResult.failed() == true.
///        On all other cases this function throws.
////////////////////////////////////////////////////////////////////////////////

SingleServerEdgeCursor::SingleServerEdgeCursor(
    BaseOptions* opts, aql::Variable const* tmpVar,
    std::vector<size_t> const* mapping,
    std::vector<BaseOptions::LookupInfo> const& lookupInfo)
    : _opts(opts),
      _trx(opts->trx()),
      _tmpVar(tmpVar),
      _currentCursor(0),
      _currentSubCursor(0),
      _cachePos(0),
      _internalCursorMapping(mapping),
      _lookupInfo(lookupInfo) {
  _cache.reserve(1000);
  TRI_ASSERT(_opts->cache() != nullptr);

  if (_opts->cache() == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "no cache present for single server edge cursor");
  }
}

SingleServerEdgeCursor::~SingleServerEdgeCursor() = default;

#ifdef USE_ENTERPRISE
static bool CheckInaccessible(transaction::Methods* trx, VPackSlice edge) {
  // for skipInaccessibleCollections we need to check the edge
  // document, in that case nextWithExtra has no benefit
  TRI_ASSERT(edge.isString());
  std::string_view str(edge.stringView());
  size_t pos = str.find('/');
  TRI_ASSERT(pos != std::string_view::npos);
  return trx->isInaccessibleCollection(str.substr(0, pos));
}
#endif

void SingleServerEdgeCursor::getDocAndRunCallback(
    IndexIterator* cursor, EdgeCursor::Callback const& callback) {
  auto collection = cursor->collection();
  EdgeDocumentToken etkn(collection->id(), _cache[_cachePos++]);
  collection->getPhysical()->read(
      _trx, etkn.localDocumentId(),
      [&](LocalDocumentId const&, VPackSlice edgeDoc) {
#ifdef USE_ENTERPRISE
        if (_trx->skipInaccessible()) {
          // TODO: we only need to check one of these
          VPackSlice from =
              transaction::helpers::extractFromFromDocument(edgeDoc);
          VPackSlice to = transaction::helpers::extractToFromDocument(edgeDoc);
          if (CheckInaccessible(_trx, from) || CheckInaccessible(_trx, to)) {
            return false;
          }
        }
#endif
        _opts->cache()->incrDocuments();
        if (_internalCursorMapping != nullptr) {
          TRI_ASSERT(_currentCursor < _internalCursorMapping->size());
          callback(std::move(etkn), edgeDoc,
                   _internalCursorMapping->at(_currentCursor));
        } else {
          callback(std::move(etkn), edgeDoc, _currentCursor);
        }
        return true;
      },
      ReadOwnWrites::no);
}

bool SingleServerEdgeCursor::advanceCursor(
    IndexIterator*& cursor, std::vector<CursorInfo>*& cursorSet) {
  TRI_ASSERT(!_cursors.empty());
  ++_currentSubCursor;
  if (_currentSubCursor >= cursorSet->size()) {
    ++_currentCursor;
    _currentSubCursor = 0;
    if (_currentCursor == _cursors.size()) {
      // We are done, all cursors exhausted.
      return false;
    }
    cursorSet = &_cursors[_currentCursor];
  }

  cursor = (*cursorSet)[_currentSubCursor].cursor.get();
  // If we switch the cursor. We have to clear the cache.
  _cache.clear();
  return true;
}

bool SingleServerEdgeCursor::next(EdgeCursor::Callback const& callback) {
  // fills callback with next EdgeDocumentToken and Slice that contains the
  // other side of the edge (we are standing on a node and want to iterate all
  // connected edges
  TRI_ASSERT(!_cursors.empty());

  // curiously enough, this method is only called in the cluster, but not
  // on single servers.

  if (_currentCursor == _cursors.size()) {
    return false;
  }

  // There is still something in the cache
  if (_cachePos < _cache.size()) {
    // get the collection
    getDocAndRunCallback(
        _cursors[_currentCursor][_currentSubCursor].cursor.get(), callback);
    return true;
  }

  // We need to refill the cache.
  _cachePos = 0;
  auto* cursorSet = &_cursors[_currentCursor];

  // get current cursor
  auto cursor = (*cursorSet)[_currentSubCursor].cursor.get();
  uint16_t coveringPosition =
      (*cursorSet)[_currentSubCursor].coveringIndexPosition;
  TRI_ASSERT(cursor != nullptr);

  // NOTE: We cannot clear the cache,
  // because the cursor expects it to be filled.
  do {
    if (cursorSet->empty() || !cursor->hasMore()) {
      if (!advanceCursor(cursor, cursorSet)) {
        return false;
      }
    } else {
      if (aql::Projections::isCoveringIndexPosition(coveringPosition)) {
        bool operationSuccessful = false;
        cursor->nextCovering(
            [&](LocalDocumentId const& token,
                IndexIteratorCoveringData& covering) {
              TRI_ASSERT(covering.isArray());
              VPackSlice edge = covering.at(coveringPosition);
              TRI_ASSERT(edge.isString());

              if (token.isSet()) {
#ifdef USE_ENTERPRISE
                if (_trx->skipInaccessible() && CheckInaccessible(_trx, edge)) {
                  return false;
                }
#endif
                operationSuccessful = true;
                auto etkn =
                    EdgeDocumentToken(cursor->collection()->id(), token);
                if (_internalCursorMapping != nullptr) {
                  TRI_ASSERT(_currentCursor < _internalCursorMapping->size());
                  callback(std::move(etkn), edge,
                           _internalCursorMapping->at(_currentCursor));
                } else {
                  callback(std::move(etkn), edge, _currentCursor);
                }
                return true;
              }
              return false;
            },
            1);
        // TODO: why are we calling this for just one document, but the
        // non-covering part we call for 1000 documents at a time?
        if (operationSuccessful) {
          return true;
        }
      } else {
        _cache.clear();
        bool tmp = cursor->next(
            [&](LocalDocumentId const& token) {
              if (token.isSet()) {
                // Document found
                _cache.emplace_back(token);
                return true;
              }
              return false;
            },
            1000);
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
  TRI_ASSERT(!_cursors.empty());
  size_t cursorId = 0;
  for (_currentCursor = 0; _currentCursor < _cursors.size(); ++_currentCursor) {
    if (_internalCursorMapping != nullptr) {
      TRI_ASSERT(_currentCursor < _internalCursorMapping->size());
      cursorId = _internalCursorMapping->at(_currentCursor);
    } else {
      cursorId = _currentCursor;
    }
    auto& cursorSet = _cursors[_currentCursor];
    for (auto& [cursor, coveringPosition] : cursorSet) {
      LogicalCollection* collection = cursor->collection();
      auto cid = collection->id();

      if (aql::Projections::isCoveringIndexPosition(coveringPosition)) {
        // thanks AppleClang for having to declare this extra variable!
        uint16_t cv = coveringPosition;

        cursor->allCovering([&](LocalDocumentId const& token,
                                IndexIteratorCoveringData& covering) {
          TRI_ASSERT(covering.isArray());
          VPackSlice edge = covering.at(cv);
          TRI_ASSERT(edge.isString());

#ifdef USE_ENTERPRISE
          if (_trx->skipInaccessible() && CheckInaccessible(_trx, edge)) {
            return false;
          }
#endif
          _opts->cache()->incrDocuments();
          callback(EdgeDocumentToken(cid, token), edge, cursorId);
          return true;
        });
      } else {
        cursor->all([&](LocalDocumentId const& token) {
          return collection->getPhysical()
              ->read(
                  _trx, token,
                  [&](LocalDocumentId const&, VPackSlice edgeDoc) {
#ifdef USE_ENTERPRISE
                    if (_trx->skipInaccessible()) {
                      // TODO: we only need to check one of these
                      VPackSlice from =
                          transaction::helpers::extractFromFromDocument(
                              edgeDoc);
                      VPackSlice to =
                          transaction::helpers::extractToFromDocument(edgeDoc);
                      if (CheckInaccessible(_trx, from) ||
                          CheckInaccessible(_trx, to)) {
                        return false;
                      }
                    }
#endif
                    _opts->cache()->incrDocuments();
                    callback(EdgeDocumentToken(cid, token), edgeDoc, cursorId);
                    return true;
                  },
                  ReadOwnWrites::no)
              .ok();
        });
      }

      // update cache hits and misses
      auto [ch, cm] = cursor->getAndResetCacheStats();
      _opts->cache()->incrCacheHits(ch);
      _opts->cache()->incrCacheMisses(cm);
    }
  }
}

void SingleServerEdgeCursor::rearm(std::string_view vertex,
                                   uint64_t /*depth*/) {
  _currentCursor = 0;
  _currentSubCursor = 0;
  _cache.clear();
  _cachePos = 0;

  if (_cursors.empty()) {
    buildLookupInfo(vertex);
    return;
  }

  size_t i = 0;
  for (auto& info : _lookupInfo) {
    ::PrepareIndexCondition(info, vertex);
    IndexIteratorOptions defaultIndexIteratorOptions;

    auto& csrs = _cursors[i++];
    size_t j = 0;
    auto& node = info.indexCondition;
    for (auto const& it : info.idxHandles) {
      auto& cursor = csrs[j].cursor;

      // steal cache hits and misses before the cursor is recycled
      auto [ch, cm] = cursor->getAndResetCacheStats();
      _opts->cache()->incrCacheHits(ch);
      _opts->cache()->incrCacheMisses(cm);

      // check if the underlying index iterator supports rearming
      if (cursor->canRearm()) {
        // rearming supported
        _opts->cache()->incrCursorsRearmed();
        if (!cursor->rearm(node, _tmpVar, defaultIndexIteratorOptions)) {
          cursor =
              std::make_unique<EmptyIndexIterator>(cursor->collection(), _trx);
        }
      } else {
        // rearming not supported - we need to throw away the index iterator
        // and create a new one
        _opts->cache()->incrCursorsCreated();
        cursor = _trx->indexScanForCondition(
            it, node, _tmpVar, defaultIndexIteratorOptions, ReadOwnWrites::no,
            static_cast<int>(
                info.conditionNeedUpdate
                    ? info.conditionMemberToUpdate
                    : transaction::Methods::kNoMutableConditionIdx));
      }
      ++j;
    }
  }
}

void SingleServerEdgeCursor::buildLookupInfo(std::string_view vertex) {
  TRI_ASSERT(_cursors.empty());
  _cursors.reserve(_lookupInfo.size());

  if (_internalCursorMapping == nullptr) {
    for (auto& info : _lookupInfo) {
      addCursor(info, vertex);
    }
  } else {
    for (auto& index : *_internalCursorMapping) {
      TRI_ASSERT(index < _lookupInfo.size());
      auto& info = _lookupInfo[index];
      addCursor(info, vertex);
    }
  }
  TRI_ASSERT(_internalCursorMapping == nullptr ||
             _internalCursorMapping->size() == _cursors.size());
}

void SingleServerEdgeCursor::addCursor(BaseOptions::LookupInfo const& info,
                                       std::string_view vertex) {
  ::PrepareIndexCondition(info, vertex);
  IndexIteratorOptions defaultIndexIteratorOptions;

  _cursors.emplace_back();
  auto& csrs = _cursors.back();
  csrs.reserve(info.idxHandles.size());
  for (std::shared_ptr<Index> const& index : info.idxHandles) {
    uint16_t coveringPosition = aql::Projections::kNoCoveringIndexPosition;

    // projections we want to cover
    aql::Projections edgeProjections(std::vector<aql::AttributeNamePath>(
        {StaticStrings::FromString, StaticStrings::ToString}));

    if (index->covers(edgeProjections)) {
      // find opposite attribute
      edgeProjections.setCoveringContext(index->collection().id(), index);

      TRI_edge_direction_e dir = info.direction;
      TRI_ASSERT(dir == TRI_EDGE_IN || dir == TRI_EDGE_OUT);

      if (dir == TRI_EDGE_OUT) {
        coveringPosition = edgeProjections.coveringIndexPosition(
            aql::AttributeNamePath::Type::ToAttribute);
      } else {
        coveringPosition = edgeProjections.coveringIndexPosition(
            aql::AttributeNamePath::Type::FromAttribute);
      }

      TRI_ASSERT(aql::Projections::isCoveringIndexPosition(coveringPosition));
    }

    csrs.emplace_back(
        _trx->indexScanForCondition(
            index, info.indexCondition, _tmpVar, defaultIndexIteratorOptions,
            ReadOwnWrites::no,
            static_cast<int>(
                info.conditionNeedUpdate
                    ? info.conditionMemberToUpdate
                    : transaction::Methods::kNoMutableConditionIdx)),
        coveringPosition);
  }
}
