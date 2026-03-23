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

#include <string_view>
#include <vector>
#include <optional>

#include "Aql/TraversalStats.h"
#include "Graph/BaseOptions.h"
#include "Graph/Cursors/EdgeCursor.h"
#include "Aql/AqlValue.h"
#include "Graph/EdgeDocumentToken.h"
#include "Graph/Steps/SingleServerProviderStep.h"
#include "StorageEngine/PhysicalCollection.h"
#include "VocBase/Identifiers/DataSourceId.h"
#include "VocBase/Identifiers/LocalDocumentId.h"

namespace arangodb {
class LocalDocumentId;
struct ResourceMonitor;
class Index;
class IndexIterator;
class IndexIteratorCoveringData;

namespace aql {
struct Variable;
struct AstNode;
}  // namespace aql

namespace transaction {
class Methods;
}

namespace graph {
struct NeighbourEntry {
  LocalDocumentId edge;
  velocypack::SharedSlice projections;
};
struct StorageNeighbourEnumerator {
  // if there are elements in PR, then set coveringPositions empty
  // because this is done in other stage
  StorageNeighbourEnumerator(std::unique_ptr<IndexIterator> indexIterator,
                             std::vector<uint16_t> coveringPositions,
                             std::uint64_t batchSize)
      : _indexIterator{std::move(indexIterator)},
        _coveringPositions{std::move(coveringPositions)},
        _batchSize{batchSize} {}

  std::unique_ptr<IndexIterator> _indexIterator;
  std::vector<uint16_t> _coveringPositions;
  std::uint64_t _batchSize;
  auto next() -> std::vector<NeighbourEntry> {
    std::vector<NeighbourEntry> result;
    result.reserve(_batchSize);
    _indexIterator->nextCovering(
        [&result, this](LocalDocumentId token,
                        IndexIteratorCoveringData& covering) {
          if (token.isSet()) {
            result.emplace_back(NeighbourEntry{
                .edge = token,
                .projections = {covering.at(
                    _coveringPositions[0]) /* for all positions */}});
          }
        },
        _batchSize);
    return result;
  }
};
struct NeighbourToStepAdapter {
  NeighbourToStepAdapter();
  DataSourceId _dataSource;
  size_t _previousStepPositionInStore;

  auto process(std::vector<NeighbourEntry> entries)
      -> std::vector<SingleServerProviderStep> {
    std::vector<SingleServerProviderStep> result;
    for (auto const& entry : entries) {
      result.emplace_back(
          SingleServerProviderStep{VertexRef{entry.projections.get("_to")},
                                   EdgeDocumentToken{_dataSource, entry.edge},
                                   _previousStepPositionInStore});
    }
    return result;
  }
};
struct NeighbourFilter {
  Expression _filter /* non covered by index */;
  transaction::Methods* _trx;
  PhysicalCollection* _collection;
  auto process(std::vector<NeighbourEntry> entries)
      -> std::vector<NeighbourEntry> {
    std::vector<SingleServerProviderStep> result;
    for (auto const& entry : entries) {
      _collection->lookup(
          _trx, entry.edge,
          [&result, entry](LocalDocumentId token, aql::DocumentData&&,
                           VPackSlice edgeDoc) {
            if (/* apply filter on edgeDoc (perhaps documentData */) {
              result.emplace_back(entry);
            }
          },
          {.countBytes = true});
    }
  }
};

struct DBServerIndexCursor {
  void all(EdgeCursor::Callback const& callback);
  bool next(EdgeCursor::Callback const& callback);
  uint64_t nextBatch(EdgeCursor::Callback const& callback, uint64_t batchSize);
  void rearm(std::string_view vertex);
  bool hasMore() const;

  DBServerIndexCursor(std::shared_ptr<arangodb::Index> idxHandle,
                      size_t cursorId, uint16_t coveringIndexPosition,
                      aql::AstNode* indexCondition,
                      std::optional<size_t> conditionMemberToUpdate,
                      transaction::Methods* trx,
                      std::shared_ptr<aql::TraversalStats> stats,
                      aql::Variable const* tmpVar, ResourceMonitor& monitor)
      : _idxHandle{idxHandle},
        _cursorId{cursorId},
        _coveringIndexPosition{coveringIndexPosition},
        _indexCondition{indexCondition},
        _conditionMemberToUpdate{conditionMemberToUpdate},
        _trx{trx},
        _tmpVar{tmpVar},
        _monitor{monitor},
        _stats{stats} {
    _cache.reserve(1000);
  }

 private:
  void prepareIndexCondition(std::string_view vertex);
  std::function<bool(LocalDocumentId, IndexIteratorCoveringData&)>
  coveringCallback(uint64_t& operationSuccessful, DataSourceId const& sourceId,
                   size_t cursorId, uint16_t coveringPosition,
                   EdgeCursor::Callback const& callback);
  std::function<bool(LocalDocumentId, aql::DocumentData&&, VPackSlice)>
  nonCoveringCallback(DataSourceId const& sourceId, size_t cursorId,
                      EdgeCursor::Callback const& callback);
  uint64_t executeOnCache(EdgeCursor::Callback const& callback,
                          uint64_t itemSize);

  std::unique_ptr<IndexIterator> _cursor;
  std::shared_ptr<arangodb::Index> _idxHandle;  // needed for rearming
  DataSourceId _collectionId;
  size_t _cursorId;

  uint16_t _coveringIndexPosition;
  aql::AstNode* _indexCondition;  // for all cursors in on collection // FI
  std::optional<size_t> _conditionMemberToUpdate;  // "

  transaction::Methods* _trx;
  aql::Variable const* _tmpVar;  // only needed in ctor and rearm
  ResourceMonitor& _monitor;     // only needed in ctor and rearm

  std::vector<LocalDocumentId> _cache;
  size_t _cachePos = 0;

  std::shared_ptr<aql::TraversalStats> _stats;
};

auto createDBServerIndexCursors(
    std::vector<BaseOptions::LookupInfo> const& lookupInfos,
    aql::Variable const* tmpVar, transaction::Methods* trx,
    std::shared_ptr<aql::TraversalStats> stats, ResourceMonitor& monitor)
    -> std::vector<DBServerIndexCursor>;

}  // namespace graph
}  // namespace arangodb
