////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include "Aql/OptimizerUtils.h"
#include "Cluster/ClusterMethods.h"
#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "Transaction/Methods.h"
#include "VocBase/AccessMode.h"

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

/// @brief Get edges for the given direction and start vertex.
std::unique_ptr<arangodb::IndexIterator> EdgeCollectionInfo::getEdges(std::string const& vertexId) {
  
  /// @brief index used for iteration
  transaction::Methods::IndexHandle indexId;
 
  _trx->addCollectionAtRuntime(_collectionName, AccessMode::Type::READ);
  auto doc = _trx->documentCollection(_collectionName);
  
  for (std::shared_ptr<arangodb::Index> const& idx : doc->getIndexes()) {
    if (idx->type() == arangodb::Index::TRI_IDX_TYPE_EDGE_INDEX) {
      auto fields = idx->fieldNames();
      if (fields.size() == 1 && fields[0].size() == 1 &&
          fields[0][0] == StaticStrings::FromString) {
        indexId = idx;
        break;
      }
    }
  }
  TRI_ASSERT(indexId != nullptr);  // We always have an edge Index
  
  _searchBuilder.setVertexId(vertexId);
  IndexIteratorOptions opts;
  opts.enableCache = false;
  return _trx->indexScanForCondition(indexId, _searchBuilder.getOutboundCondition(), _searchBuilder.getVariable(), opts);
}

/// @brief Return name of the wrapped collection
std::string const& EdgeCollectionInfo::getName() const {
  return _collectionName;
}
