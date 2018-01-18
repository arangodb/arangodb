////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_ROCKSDB_GEO_S2_INDEX_H
#define ARANGOD_ROCKSDB_GEO_S2_INDEX_H 1

#include "Basics/Result.h"
#include "Geo/Index.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "VocBase/voc-types.h"

#include <geometry/s2cellid.h>
#include <velocypack/Builder.h>

class S2Region;

namespace arangodb {
class RocksDBGeoS2Index final : public arangodb::RocksDBIndex, public geo::Index {
  friend class RocksDBSphericalIndexIterator;

 public:
  RocksDBGeoS2Index() = delete;

  RocksDBGeoS2Index(TRI_idx_iid_t, arangodb::LogicalCollection*,
                    velocypack::Slice const&);

  ~RocksDBGeoS2Index() override {}

 public:
  IndexType type() const override { return TRI_IDX_TYPE_S2_INDEX; }

  char const* typeName() const override { return "s2index"; }

  IndexIterator* iteratorForCondition(transaction::Methods*,
                                      ManagedDocumentResult*,
                                      arangodb::aql::AstNode const*,
                                      arangodb::aql::Variable const*,
                                      IndexIteratorOptions const&) override;

  bool allowExpansion() const override { return false; }

  bool canBeDropped() const override { return true; }

  bool isSorted() const override { return false; }

  bool hasSelectivityEstimate() const override { return false; }

  void toVelocyPack(velocypack::Builder&, bool, bool) const override;
  // Uses default toVelocyPackFigures

  bool matchesDefinition(velocypack::Slice const& info) const override;

  /// insert index elements into the specified write batch.
  Result insertInternal(transaction::Methods* trx, RocksDBMethods*,
                        LocalDocumentId const& documentId,
                        arangodb::velocypack::Slice const&,
                        OperationMode mode) override;

  /// remove index elements and put it in the specified write batch.
  Result removeInternal(transaction::Methods*, RocksDBMethods*,
                        LocalDocumentId const& documentId,
                        arangodb::velocypack::Slice const&,
                        OperationMode mode) override;
};
}  // namespace arangodb

#endif
