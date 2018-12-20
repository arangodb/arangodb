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

#ifndef ARANGOD_ROCKSDB_GEO_INDEX_H
#define ARANGOD_ROCKSDB_GEO_INDEX_H 1

#include "Basics/Result.h"
#include "GeoIndex/Index.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "VocBase/voc-types.h"

#include <s2/s2cell_id.h>
#include <velocypack/Builder.h>

class S2Region;

namespace arangodb {
class RocksDBGeoIndex final : public RocksDBIndex, public geo_index::Index {
  friend class RocksDBSphericalIndexIterator;

 public:
  RocksDBGeoIndex() = delete;

  RocksDBGeoIndex(
    TRI_idx_iid_t iid,
    arangodb::LogicalCollection& collection,
    arangodb::velocypack::Slice const& info,
    std::string const& typeName
  );

  ~RocksDBGeoIndex() override {}

  IndexType type() const override {
    if ("geo1" == _typeName) {
      return TRI_IDX_TYPE_GEO1_INDEX;
    } else if ("geo2" == _typeName) {
      return TRI_IDX_TYPE_GEO2_INDEX;
    }
    return TRI_IDX_TYPE_GEO_INDEX;
  }

  bool pointsOnly() const {
    return (_typeName != "geo");
  }

  char const* typeName() const override { return _typeName.c_str(); }

  IndexIterator* iteratorForCondition(transaction::Methods*,
                                      ManagedDocumentResult*,
                                      arangodb::aql::AstNode const*,
                                      arangodb::aql::Variable const*,
                                      IndexIteratorOptions const&) override;

  bool canBeDropped() const override { return true; }

  bool isSorted() const override { return false; }

  bool hasSelectivityEstimate() const override { return false; }

  void toVelocyPack(velocypack::Builder&,
    std::underlying_type<arangodb::Index::Serialize>::type) const override;

  bool matchesDefinition(velocypack::Slice const& info) const override;

  /// insert index elements into the specified write batch.
  Result insertInternal(
    transaction::Methods& trx,
    RocksDBMethods* methods,
    LocalDocumentId const& documentId,
    velocypack::Slice const& doc,
    arangodb::Index::OperationMode mode
  ) override;

  /// remove index elements and put it in the specified write batch.
  Result removeInternal(
    transaction::Methods& trx,
    RocksDBMethods* methods,
    LocalDocumentId const& documentId,
    velocypack::Slice const& docs,
    arangodb::Index::OperationMode mode
  ) override;

 private:
  std::string const _typeName;
};
}  // namespace arangodb

#endif