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

#include "IndexHelpers.h"

#include "Cluster/ClusterMethods.h"
#include "Transaction/Methods.h"
#include "Utils/OperationCursor.h"

using namespace arangodb;
using namespace arangodb::traverser;

EdgeCollectionInfo::EdgeCollectionInfo(transaction::Methods* trx,
                                       std::string const& collectionName)
    : _trx(trx),
      _collectionName(collectionName),
      _searchBuilder() {

  if (!trx->isEdgeCollection(collectionName)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get edges for the given direction and start vertex.
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<arangodb::OperationCursor> EdgeCollectionInfo::getEdges(
    std::string const& vertexId, arangodb::ManagedDocumentResult* mmdr) {
  
  /// @brief index used for iteration
  transaction::Methods::IndexHandle indexId;
  
  auto var = _searchBuilder.getVariable();
  auto cond = _searchBuilder.getOutboundCondition();
  bool worked = _trx->getBestIndexHandleForFilterCondition(_collectionName, cond,
                                                           var, 1000, indexId);
  TRI_ASSERT(worked);  // We always have an edge Index
  
  _searchBuilder.setVertexId(vertexId);
  std::unique_ptr<arangodb::OperationCursor> res;
  IndexIteratorOptions opts;
  opts.enableCache = false;
  res.reset(_trx->indexScanForCondition(indexId,
                                        _searchBuilder.getOutboundCondition(),
                                        _searchBuilder.getVariable(), mmdr, opts));
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Return name of the wrapped collection
////////////////////////////////////////////////////////////////////////////////

std::string const& EdgeCollectionInfo::getName() const {
  return _collectionName;
}
