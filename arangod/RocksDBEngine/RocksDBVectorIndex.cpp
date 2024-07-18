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
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBVectorIndex.h"
#include "RocksDBIndex.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include "Indexes/Index.h"
#include "VocBase/LogicalCollection.h"

namespace arangodb {

RocksDBVectorIndex::RocksDBVectorIndex(IndexId iid, LogicalCollection& coll,
                                       arangodb::velocypack::Slice const& info)
    : RocksDBIndex(iid, coll, info,
                   RocksDBColumnFamilyManager::get(
                       RocksDBColumnFamilyManager::Family::VectorIndex),
                   /*useCache*/ false,
                   /*cacheManager*/ nullptr,
                   /*engine*/
                   coll.vocbase().engine<RocksDBEngine>()) {
  TRI_ASSERT(type() == Index::TRI_IDX_TYPE_VECTOR_INDEX);
}

/// @brief Test if this index matches the definition
bool RocksDBVectorIndex::matchesDefinition(VPackSlice const& info) const {
  return false;
}

void RocksDBVectorIndex::toVelocyPack(
    arangodb::velocypack::Builder& builder,
    std::underlying_type<Index::Serialize>::type flags) const {
  // TODO
}

/// @brief inserts a document into the index
Result RocksDBVectorIndex::insert(transaction::Methods& trx,
                                  RocksDBMethods* mthds,
                                  LocalDocumentId documentId,
                                  velocypack::Slice doc,
                                  OperationOptions const& options,
                                  bool performChecks) {
  // TODO
  return Result();
}

/// @brief removes a document from the index
Result RocksDBVectorIndex::remove(transaction::Methods& trx,
                                  RocksDBMethods* mthds,
                                  LocalDocumentId documentId,
                                  velocypack::Slice doc,
                                  OperationOptions const& options) {
  // TODO
  return Result();
}

}  // namespace arangodb
