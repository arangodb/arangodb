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

#ifndef ARANGOD_MMFILES_GEO_S2_INDEX_H
#define ARANGOD_MMFILES_GEO_S2_INDEX_H 1

#include "Geo/GeoParams.h"
#include "GeoIndex/Index.h"
#include "MMFiles/MMFilesIndex.h"
#include "VocBase/LocalDocumentId.h"

#include <btree/btree_map.h>
#include <s2/s2cell_id.h>
#include <velocypack/Builder.h>

namespace arangodb {

class MMFilesGeoS2Index final : public MMFilesIndex, public geo_index::Index {
 public:
  MMFilesGeoS2Index() = delete;

  MMFilesGeoS2Index(TRI_idx_iid_t, LogicalCollection*,
                    arangodb::velocypack::Slice const&);

 public:

  struct IndexValue {
    IndexValue() : documentId(0), centroid(-1, -1) {}
    IndexValue(LocalDocumentId lid, geo::Coordinate&& c)
        : documentId(lid), centroid(c) {}
    LocalDocumentId documentId;
    geo::Coordinate centroid;
  };

  typedef btree::btree_multimap<S2CellId, IndexValue> IndexTree;

 public:
  constexpr IndexType type() const override { return TRI_IDX_TYPE_S2_INDEX; }

  constexpr char const* typeName() const override { return "s2index"; }

  constexpr bool allowExpansion() const override { return false; }

  constexpr bool canBeDropped() const override { return true; }

  constexpr bool isSorted() const override { return false; }

  constexpr bool hasSelectivityEstimate() const override { return false; }

  size_t memory() const override;

  void toVelocyPack(velocypack::Builder&, bool withFigures,
                    bool forPersistence) const override;
  // Uses default toVelocyPackFigures

  bool matchesDefinition(velocypack::Slice const& info) const override;

  Result insert(transaction::Methods*, LocalDocumentId const& documentId,
                arangodb::velocypack::Slice const&,
                OperationMode mode) override;

  Result remove(transaction::Methods*, LocalDocumentId const& documentId,
                arangodb::velocypack::Slice const&,
                OperationMode mode) override;

  IndexIterator* iteratorForCondition(transaction::Methods*,
                                      ManagedDocumentResult*,
                                      arangodb::aql::AstNode const*,
                                      arangodb::aql::Variable const*,
                                      IndexIteratorOptions const&) override;

  void load() override {}
  void unload() override;

  IndexTree const& tree() const { return _tree; }

 private:

  MMFilesGeoS2Index::IndexTree _tree;
};
}

#endif
