////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_MMFILES_GEO_INDEX_H
#define ARANGOD_MMFILES_GEO_INDEX_H 1

#include "Geo/GeoParams.h"
#include "GeoIndex/Index.h"
#include "MMFiles/MMFilesIndex.h"
#include "VocBase/LocalDocumentId.h"

#include <s2/s2cell_id.h>
#include <s2/s2point.h>
#include <s2/util/gtl/btree_map.h>
#include <velocypack/Builder.h>

namespace arangodb {

class MMFilesGeoIndex final : public MMFilesIndex, public geo_index::Index {
 public:
  MMFilesGeoIndex() = delete;

  MMFilesGeoIndex(
    TRI_idx_iid_t iid,
    LogicalCollection& collection,
    arangodb::velocypack::Slice const& info,
    std::string const& typeName
  );

  struct IndexValue {
    IndexValue() : documentId(0), centroid(0,0,0) {}
    IndexValue(LocalDocumentId lid, S2Point&& c)
        : documentId(lid), centroid(c) {}
    LocalDocumentId documentId;
    S2Point centroid;
  };

  typedef gtl::btree_multimap<S2CellId, IndexValue> IndexTree;

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

  bool canBeDropped() const override { return true; }

  bool isSorted() const override { return false; }

  bool hasSelectivityEstimate() const override { return false; }

  size_t memory() const override;

  using arangodb::Index::toVelocyPack;
  void toVelocyPack(velocypack::Builder&,
                    std::underlying_type<arangodb::Index::Serialize>::type) const override;

  bool matchesDefinition(velocypack::Slice const& info) const override;

  Result insert(
    transaction::Methods& trx,
    LocalDocumentId const& documentId,
    velocypack::Slice const& doc,
    arangodb::Index::OperationMode mode
  ) override;

  Result remove(
    transaction::Methods& trx,
    LocalDocumentId const& documentId,
    velocypack::Slice const& doc,
    arangodb::Index::OperationMode mode
  ) override;

  IndexIterator* iteratorForCondition(transaction::Methods*,
                                      ManagedDocumentResult*,
                                      arangodb::aql::AstNode const*,
                                      arangodb::aql::Variable const*,
                                      IndexIteratorOptions const&) override;

  void load() override {}
  void unload() override;

  IndexTree const& tree() const { return _tree; }

 private:
  MMFilesGeoIndex::IndexTree _tree;
  std::string const _typeName;
};
}  // namespace arangodb

#endif