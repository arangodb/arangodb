////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#include "EdgeCollectionInfo.h"
#include "Cluster/ClusterMethods.h"
#include "Transaction/Methods.h"
#include "Utils/OperationCursor.h"

using namespace arangodb;
using namespace arangodb::traverser;

EdgeCollectionInfo::EdgeCollectionInfo(transaction::Methods* trx,
                                       std::string const& collectionName,
                                       TRI_edge_direction_e const direction,
                                       std::string const& weightAttribute, double defaultWeight)
    : _trx(trx),
      _collectionName(collectionName),
      _searchBuilder(),
      _weightAttribute(weightAttribute),
      _dir(direction) {
  TRI_ASSERT(_dir == TRI_EDGE_OUT || _dir == TRI_EDGE_IN);

  if (!trx->isEdgeCollection(collectionName)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID);
  }

  auto var = _searchBuilder.getVariable();
  if (_dir == TRI_EDGE_OUT) {
    auto cond = _searchBuilder.getOutboundCondition();
    bool worked =
        _trx->getBestIndexHandleForFilterCondition(_collectionName, cond, var, 1000,
                                                   aql::IndexHint(), _forwardIndexId);
    TRI_ASSERT(worked);  // We always have an edge Index
    cond = _searchBuilder.getInboundCondition();
    worked = _trx->getBestIndexHandleForFilterCondition(_collectionName, cond, var,
                                                        1000, aql::IndexHint(),
                                                        _backwardIndexId);
    TRI_ASSERT(worked);  // We always have an edge Index
  } else {
    auto cond = _searchBuilder.getInboundCondition();
    bool worked =
        _trx->getBestIndexHandleForFilterCondition(_collectionName, cond, var, 1000,
                                                   aql::IndexHint(), _forwardIndexId);
    TRI_ASSERT(worked);  // We always have an edge Index
    cond = _searchBuilder.getOutboundCondition();
    worked = _trx->getBestIndexHandleForFilterCondition(_collectionName, cond, var,
                                                        1000, aql::IndexHint(),
                                                        _backwardIndexId);
    TRI_ASSERT(worked);  // We always have an edge Index
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get edges for the given direction and start vertex.
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<arangodb::OperationCursor> EdgeCollectionInfo::getEdges(
    std::string const& vertexId) {
  _searchBuilder.setVertexId(vertexId);
  arangodb::aql::AstNode const* cond;
  if (_dir == TRI_EDGE_OUT) {
    cond = _searchBuilder.getOutboundCondition();
  } else {
    cond = _searchBuilder.getInboundCondition();
  }
    
  IndexIteratorOptions opts;
  return std::make_unique<OperationCursor>(_trx->indexScanForCondition(
      _forwardIndexId, cond, _searchBuilder.getVariable(), opts));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Return name of the wrapped collection
////////////////////////////////////////////////////////////////////////////////

std::string const& EdgeCollectionInfo::getName() const {
  return _collectionName;
}
