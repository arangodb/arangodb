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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <velocypack/Builder.h>

#include "Basics/Result.h"
#include "GeoIndex/Index.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "VocBase/Identifiers/IndexId.h"
#include "VocBase/voc-types.h"

namespace arangodb {

class RocksDBGeoIndex final : public RocksDBIndex, public geo_index::Index {
 public:
  RocksDBGeoIndex() = delete;

  RocksDBGeoIndex(IndexId iid, arangodb::LogicalCollection& collection,
                  arangodb::velocypack::Slice info,
                  std::string const& typeName);

  ~RocksDBGeoIndex() = default;

  IndexType type() const override {
    if ("geo1" == _typeName) {
      return TRI_IDX_TYPE_GEO1_INDEX;
    } else if ("geo2" == _typeName) {
      return TRI_IDX_TYPE_GEO2_INDEX;
    }
    return TRI_IDX_TYPE_GEO_INDEX;
  }

  bool pointsOnly() const { return (_typeName != "geo"); }

  char const* typeName() const override { return _typeName.c_str(); }

  std::vector<std::vector<arangodb::basics::AttributeName>> const&
  coveredFields() const override {
    // index does not cover the index attribute(s)!
    return arangodb::Index::emptyCoveredFields;
  }

  std::unique_ptr<IndexIterator> iteratorForCondition(
      ResourceMonitor& monitor, transaction::Methods* trx,
      arangodb::aql::AstNode const* node,
      arangodb::aql::Variable const* reference,
      IndexIteratorOptions const& opts, ReadOwnWrites readOwnWrites,
      int) override;

  bool canBeDropped() const override { return true; }

  bool isSorted() const override { return false; }

  bool hasSelectivityEstimate() const override { return false; }

  arangodb::Index::FilterCosts supportsFilterCondition(
      transaction::Methods& trx,
      std::vector<std::shared_ptr<arangodb::Index>> const& allIndexes,
      aql::AstNode const* node, aql::Variable const* reference,
      size_t itemsInIndex) const override;

  void toVelocyPack(
      velocypack::Builder&,
      std::underlying_type<arangodb::Index::Serialize>::type) const override;

  bool matchesDefinition(velocypack::Slice const& info) const override;

  /// insert index elements into the specified write batch.
  Result insert(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId documentId, velocypack::Slice doc,
                arangodb::OperationOptions const& /*options*/,
                bool /*performChecks*/) override;

  /// remove index elements and put it in the specified write batch.
  Result remove(transaction::Methods& trx, RocksDBMethods* methods,
                LocalDocumentId documentId, velocypack::Slice doc,
                OperationOptions const& /*options*/) override;

 private:
  std::string const _typeName;
};
}  // namespace arangodb
