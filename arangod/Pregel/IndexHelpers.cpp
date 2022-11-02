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

#include "IndexHelpers.h"

#include "Aql/AttributeNamePath.h"
#include "Aql/OptimizerUtils.h"
#include "Aql/Projections.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ClusterMethods.h"
#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "Transaction/Methods.h"
#include "VocBase/AccessMode.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;
using namespace arangodb::traverser;

EdgeCollectionInfo::EdgeCollectionInfo(transaction::Methods* trx,
                                       std::string const& collectionName)
    : _trx(trx),
      _collectionName(collectionName),
      _collection(nullptr),
      _coveringPosition(0) {
  if (!trx->isEdgeCollection(collectionName)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID);
  }

  _trx->addCollectionAtRuntime(_collectionName, AccessMode::Type::READ);

  // projections we need to cover
  aql::Projections edgeProjections(std::vector<aql::AttributeNamePath>(
      {StaticStrings::FromString, StaticStrings::ToString}));

  _collection = _trx->documentCollection(_collectionName);
  TRI_ASSERT(_collection != nullptr);

  for (std::shared_ptr<arangodb::Index> const& idx :
       _collection->getIndexes()) {
    // we currently rely on an outbound edge index, but this could be changed to
    // use a different index in the future.
    if (idx->type() == arangodb::Index::TRI_IDX_TYPE_EDGE_INDEX) {
      auto const& fields = idx->fieldNames();
      if (fields.size() == 1 && fields[0].size() == 1 &&
          fields[0][0] == StaticStrings::FromString) {
        if (idx->covers(edgeProjections)) {
          edgeProjections.setCoveringContext(_collection->id(), idx);
          // Pregel currently only supports outbound edges.
          _coveringPosition = edgeProjections.coveringIndexPosition(
              aql::AttributeNamePath::Type::ToAttribute);
        }
        _index = idx;
        break;
      }
    }
  }
  TRI_ASSERT(_index != nullptr);  // We always have an edge index

  // configure shared index iterator options
  _indexIteratorOptions.useCache = false;
}

/// @brief Get edges for the given direction and start vertex.
arangodb::IndexIterator* EdgeCollectionInfo::getEdges(
    std::string const& vertexId) {
  _searchBuilder.setVertexId(vertexId);

  if (_cursor == nullptr || !_cursor->canRearm()) {
    _cursor = _trx->indexScanForCondition(
        _index, _searchBuilder.getOutboundCondition(),
        _searchBuilder.getVariable(), _indexIteratorOptions, ReadOwnWrites::no,
        transaction::Methods::kNoMutableConditionIdx);
  } else {
    if (!_cursor->rearm(_searchBuilder.getOutboundCondition(),
                        _searchBuilder.getVariable(), _indexIteratorOptions)) {
      _cursor = std::make_unique<EmptyIndexIterator>(_collection, _trx);
    }
  }

  TRI_ASSERT(_cursor != nullptr);
  return _cursor.get();
}

/// @brief Return name of the wrapped collection
std::string const& EdgeCollectionInfo::getName() const {
  return _collectionName;
}
