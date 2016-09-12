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
#include "Indexes/EdgeIndex.h"
#include "Utils/OperationCursor.h"

using namespace arangodb::traverser;

EdgeCollectionInfo::EdgeCollectionInfo(arangodb::Transaction* trx,
                                       std::string const& collectionName,
                                       TRI_edge_direction_e const direction,
                                       std::string const& weightAttribute,
                                       double defaultWeight) 
    : _trx(trx),
      _collectionName(collectionName),
      _weightAttribute(weightAttribute),
      _defaultWeight(defaultWeight),
      _forwardDir(direction) {

  switch (direction) {
    case TRI_EDGE_OUT:
      _backwardDir = TRI_EDGE_IN;
      break;
    case TRI_EDGE_IN:
      _backwardDir = TRI_EDGE_OUT;
      break;
    case TRI_EDGE_ANY:
      _backwardDir = TRI_EDGE_ANY;
      break;
  }

  if (!trx->isEdgeCollection(collectionName)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID);
  }
  _indexId = trx->edgeIndexHandle(collectionName);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get edges for the given direction and start vertex.
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<arangodb::OperationCursor> EdgeCollectionInfo::getEdges(
    std::string const& vertexId) {
  _searchBuilder.clear();
  EdgeIndex::buildSearchValue(_forwardDir, vertexId, _searchBuilder);
  return _trx->indexScan(_collectionName,
                         arangodb::Transaction::CursorType::INDEX, _indexId,
                         _searchBuilder.slice(), 0, UINT64_MAX, 1000, false);
}

std::unique_ptr<arangodb::OperationCursor> EdgeCollectionInfo::getEdges(
    VPackSlice const& vertexId) {
  _searchBuilder.clear();
  EdgeIndex::buildSearchValue(_forwardDir, vertexId, _searchBuilder);
  return _trx->indexScan(_collectionName,
                         arangodb::Transaction::CursorType::INDEX, _indexId,
                         _searchBuilder.slice(), 0, UINT64_MAX, 1000, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get edges for the given direction and start vertex. On Coordinator.
////////////////////////////////////////////////////////////////////////////////

int EdgeCollectionInfo::getEdgesCoordinator(VPackSlice const& vertexId,
                                            VPackBuilder& result) {
  TRI_ASSERT(result.isEmpty());
  arangodb::rest::ResponseCode responseCode;
  result.openObject();
  int res = getFilteredEdgesOnCoordinator(
      _trx->vocbase()->name(), _collectionName, vertexId.copyString(),
      _forwardDir, _unused, responseCode, result);
  result.close();
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get edges for the given direction and start vertex. Reverse version
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<arangodb::OperationCursor> EdgeCollectionInfo::getReverseEdges(
    std::string const& vertexId) {
  _searchBuilder.clear();
  EdgeIndex::buildSearchValue(_backwardDir, vertexId, _searchBuilder);
  return _trx->indexScan(_collectionName,
                         arangodb::Transaction::CursorType::INDEX, _indexId,
                         _searchBuilder.slice(), 0, UINT64_MAX, 1000, false);
}

std::unique_ptr<arangodb::OperationCursor> EdgeCollectionInfo::getReverseEdges(
    VPackSlice const& vertexId) {
  _searchBuilder.clear();
  EdgeIndex::buildSearchValue(_backwardDir, vertexId, _searchBuilder);
  return _trx->indexScan(_collectionName,
                         arangodb::Transaction::CursorType::INDEX, _indexId,
                         _searchBuilder.slice(), 0, UINT64_MAX, 1000, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get edges for the given direction and start vertex. Reverse version on Coordinator.
////////////////////////////////////////////////////////////////////////////////

int EdgeCollectionInfo::getReverseEdgesCoordinator(VPackSlice const& vertexId,
                                                   VPackBuilder& result) {
  TRI_ASSERT(result.isEmpty());
  arangodb::rest::ResponseCode responseCode;
  result.openObject();
  int res = getFilteredEdgesOnCoordinator(
      _trx->vocbase()->name(), _collectionName, vertexId.copyString(),
      _backwardDir, _unused, responseCode, result);
  result.close();
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Compute the weight of an edge
////////////////////////////////////////////////////////////////////////////////

double EdgeCollectionInfo::weightEdge(VPackSlice const edge) {
  TRI_ASSERT(!_weightAttribute.empty());
  return arangodb::basics::VelocyPackHelper::getNumericValue<double>(
      edge, _weightAttribute.c_str(), _defaultWeight);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Return name of the wrapped collection
////////////////////////////////////////////////////////////////////////////////

std::string const& EdgeCollectionInfo::getName() const {
  return _collectionName;
}

