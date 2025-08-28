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

#include "DBServerIndexCursor.h"

#include "Aql/AstNode.h"
#include "Aql/AttributeNamePath.h"
#include "Aql/QueryContext.h"
#include "Aql/Projections.h"
#include "Basics/StaticStrings.h"
#include "Graph/BaseOptions.h"
#include "Graph/EdgeDocumentToken.h"
#include "Graph/TraverserCache.h"
#include "Indexes/IndexIterator.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/Identifiers/DataSourceId.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;
using namespace arangodb::graph;

namespace {
#ifdef USE_ENTERPRISE
bool CheckInaccessible(transaction::Methods* trx, VPackSlice edge) {
  // for skipInaccessibleCollections we need to check the edge
  // document, in that case nextWithExtra has no benefit
  TRI_ASSERT(edge.isString());
  std::string_view str(edge.stringView());
  size_t pos = str.find('/');
  TRI_ASSERT(pos != std::string_view::npos);
  return trx->isInaccessibleCollection(str.substr(0, pos));
}
#endif

uint16_t getCoveringPosition(std::shared_ptr<Index> const& index,
                             TRI_edge_direction_e direction,
                             ResourceMonitor& monitor) {
  // projections we want to cover
  std::vector<aql::AttributeNamePath> paths = {};
  paths.emplace_back(
      aql::AttributeNamePath({StaticStrings::FromString}, monitor));
  paths.emplace_back(
      aql::AttributeNamePath({StaticStrings::ToString}, monitor));
  aql::Projections edgeProjections(std::move(paths));

  if (not index->covers(edgeProjections)) {
    return aql::Projections::kNoCoveringIndexPosition;
  }

  // find opposite attribute
  edgeProjections.setCoveringContext(index->collection().id(), index);

  TRI_ASSERT(direction == TRI_EDGE_IN || direction == TRI_EDGE_OUT);

  uint16_t coveringPosition = aql::Projections::kNoCoveringIndexPosition;
  if (direction == TRI_EDGE_OUT) {
    coveringPosition = edgeProjections.coveringIndexPosition(
        aql::AttributeNamePath::Type::ToAttribute);
  } else {
    coveringPosition = edgeProjections.coveringIndexPosition(
        aql::AttributeNamePath::Type::FromAttribute);
  }
  TRI_ASSERT(aql::Projections::isCoveringIndexPosition(coveringPosition));
  return coveringPosition;
}
}  // namespace

auto arangodb::graph::createDBServerIndexCursors(
    std::vector<BaseOptions::LookupInfo> const& lookupInfos,
    aql::Variable const* tmpVar, transaction::Methods* trx,
    TraverserCache* traverserCache, ResourceMonitor& monitor)
    -> std::vector<DBServerIndexCursor> {
  std::vector<DBServerIndexCursor> cursors;
  // there are at least lookupInfo.size() many cursors
  cursors.reserve(lookupInfos.size());
  size_t infoCount = 0;
  for (auto const& info : lookupInfos) {
    for (std::shared_ptr<Index> const& index : info.idxHandles) {
      auto coveringPosition =
          getCoveringPosition(index, info.direction, monitor);

      cursors.emplace_back(DBServerIndexCursor{
          index, infoCount, coveringPosition, info.indexCondition,
          info.conditionNeedUpdate
              ? std::optional<size_t>{info.conditionMemberToUpdate}
              : std::nullopt,
          trx, traverserCache, tmpVar, monitor});
    }
    infoCount++;
  }
  return cursors;
}

void DBServerIndexCursor::all(EdgeCursor::Callback const& callback) {
  TRI_ASSERT(_cursor != nullptr);

  if (aql::Projections::isCoveringIndexPosition(_coveringIndexPosition)) {
    bool operationSuccessful = true;
    auto cb = coveringCallback(operationSuccessful, _cursor->collection()->id(),
                               _cursorId, _coveringIndexPosition, callback);

    _cursor->allCovering(cb);
  } else {
    auto cb =
        nonCoveringCallback(_cursor->collection()->id(), _cursorId, callback);
    _cursor->all([&](LocalDocumentId token) {
      return _cursor->collection()
          ->getPhysical()
          ->lookup(_trx, token, cb, {.countBytes = true})
          .ok();
    });
  }

  // update cache hits and misses
  auto [ch, cm] = _cursor->getAndResetCacheStats();
  _traverserCache->incrCacheHits(ch);
  _traverserCache->incrCacheMisses(cm);
}

bool DBServerIndexCursor::next(EdgeCursor::Callback const& callback) {
  TRI_ASSERT(_cursor != nullptr);

  if (_cachePos < _cache.size()) {
    // get the collection
    auto cb =
        nonCoveringCallback(_cursor->collection()->id(), _cursorId, callback);
    _cursor->collection()->getPhysical()->lookup(_trx, _cache[_cachePos++], cb,
                                                 {.countBytes = true});
    return true;
  }

  // We need to refill the cache.
  _cachePos = 0;

  if (aql::Projections::isCoveringIndexPosition(_coveringIndexPosition)) {
    bool operationSuccessful = false;
    do {
      if (!_cursor->hasMore()) {
        return false;
      }
      auto cb =
          coveringCallback(operationSuccessful, _cursor->collection()->id(),
                           _cursorId, _coveringIndexPosition, callback);
      _cursor->nextCovering(cb, 1);
      if (operationSuccessful) {
        return true;
      }
    } while (!operationSuccessful);
  }

  TRI_ASSERT(
      !aql::Projections::isCoveringIndexPosition(_coveringIndexPosition));

  do {
    if (!_cursor->hasMore()) {
      return false;
    }
    _cache.clear();
    bool tmp = _cursor->next(
        [&](LocalDocumentId token) {
          if (token.isSet()) {
            _cache.emplace_back(token);
            return true;
          }
          return false;
        },
        1000);
    TRI_ASSERT(tmp == _cursor->hasMore());
  } while (_cache.empty());
  TRI_ASSERT(!_cache.empty());
  TRI_ASSERT(_cachePos < _cache.size());
  auto cb =
      nonCoveringCallback(_cursor->collection()->id(), _cursorId, callback);
  _cursor->collection()->getPhysical()->lookup(_trx, _cache[_cachePos++], cb,
                                               {.countBytes = true});
  return true;
}

void DBServerIndexCursor::rearm(std::string_view vertex) {
  _cache.clear();
  _cachePos = 0;

  prepareIndexCondition(vertex);
  IndexIteratorOptions defaultIndexIteratorOptions;
  if (_cursor == nullptr) {
    _cursor = _trx->indexScanForCondition(
        _monitor, _idxHandle, _indexCondition, _tmpVar,
        defaultIndexIteratorOptions, ReadOwnWrites::no,
        static_cast<int>(_conditionMemberToUpdate.value_or(
            transaction::Methods::kNoMutableConditionIdx)));
    return;
  }

  // steal cache hits and misses before the cursor is recycled
  auto [ch, cm] = _cursor->getAndResetCacheStats();
  _traverserCache->incrCacheHits(ch);
  _traverserCache->incrCacheMisses(cm);

  // check if the underlying index iterator supports rearming
  if (_cursor->canRearm()) {
    // rearming supported
    _traverserCache->incrCursorsRearmed();
    if (!_cursor->rearm(_indexCondition, _tmpVar,
                        defaultIndexIteratorOptions)) {
      _cursor =
          std::make_unique<EmptyIndexIterator>(_cursor->collection(), _trx);
    }
  } else {
    // rearming not supported - we need to throw away the index iterator
    // and create a new one
    _traverserCache->incrCursorsCreated();
    _cursor = _trx->indexScanForCondition(
        _monitor, _idxHandle, _indexCondition, _tmpVar,
        defaultIndexIteratorOptions, ReadOwnWrites::no,
        static_cast<int>(_conditionMemberToUpdate.value_or(
            transaction::Methods::kNoMutableConditionIdx)));
  }
}

void DBServerIndexCursor::prepareIndexCondition(std::string_view vertex) {
  auto& node = _indexCondition;
  TRI_ASSERT(node->numMembers() > 0);
  if (_conditionMemberToUpdate.has_value()) {
    // We have to inject _from/_to iff the condition needs it
    auto dirCmp = node->getMemberUnchecked(_conditionMemberToUpdate.value());
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

std::function<bool(LocalDocumentId, aql::DocumentData&&, VPackSlice)>
DBServerIndexCursor::nonCoveringCallback(DataSourceId const& sourceId,
                                         size_t cursorId,
                                         EdgeCursor::Callback const& callback) {
  return [=, this](LocalDocumentId token, aql::DocumentData&&,
                   VPackSlice edgeDoc) {
#ifdef USE_ENTERPRISE
    if (_trx->skipInaccessible()) {
      // TODO: we only need to check one of these
      VPackSlice from = transaction::helpers::extractFromFromDocument(edgeDoc);
      VPackSlice to = transaction::helpers::extractToFromDocument(edgeDoc);
      if (CheckInaccessible(_trx, from) || CheckInaccessible(_trx, to)) {
        return false;
      }
    }
#endif
    _traverserCache->incrDocuments();
    callback(EdgeDocumentToken(sourceId, token), edgeDoc, cursorId);
    return true;
  };
}
std::function<bool(LocalDocumentId, IndexIteratorCoveringData&)>
DBServerIndexCursor::coveringCallback(bool& operationSuccessful,
                                      DataSourceId const& sourceId,
                                      size_t cursorId,
                                      uint16_t coveringPosition,
                                      EdgeCursor::Callback const& callback) {
  return [=, this, &operationSuccessful](LocalDocumentId token,
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
      _traverserCache->incrDocuments();
      callback(EdgeDocumentToken(sourceId, token), edge, cursorId);
      return true;
    }
    return false;
  };
}
