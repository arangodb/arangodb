////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_ROCKSDB_S2_GEO_INDEX_H
#define ARANGOD_ROCKSDB_S2_GEO_INDEX_H 1

#include "Basics/Common.h"
#include "Indexes/IndexIterator.h"
#include "RocksDBEngine/Indexes/RocksDBIndex.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>
#include <type_traits>

namespace arangodb {
namespace rocksdbengine {

class S2GeoIndex final : public arangodb::RocksDBIndex {
  friend class S2GeoIndexIterator;

 public:
  S2GeoIndex() = delete;

  S2GeoIndex(TRI_idx_iid_t, arangodb::LogicalCollection*,
             velocypack::Slice const&);

  ~S2GeoIndex() override;

 public:
  /// @brief geo index variants
  enum class IndexVariant : uint8_t {
    NONE = 0,
    /// two distinct fields representing GeoJSON Point
    INDIVIDUAL_LAT_LON,
    /// pair [<latitude>, <longitude>] eqvivalent to GeoJSON Point
    COMBINED_LAT_LON,
    // geojson object or legacy coordinate
    // pair [<longitude>, <latitude>]. Should also support
    // other geojson object types.
    COMBINED_GEOJSON
  };
  
  enum class IteratorType : uint8_t {
    // Specifiy a location for which a geospatial query returns the documents
    // from nearest to farthest.
    NEAR,
    // Select documents with geospatial data that are located entirely within a shape.
    // When determining inclusion, we consider the border of a shape to be part of the shape,
    // subject to the precision of floating point numbers.
    WITHIN,
    // Select documents whose geospatial data intersects with a specified GeoJSON object.
    INTERSECT
  };

 public:
  
  IndexType type() const override {
    return TRI_IDX_TYPE_GEOSPATIAL_INDEX;
  }

  char const* typeName() const override {
    return "geospatial";
  }

  IndexIterator* iteratorForCondition(transaction::Methods*,
                                      ManagedDocumentResult*,
                                      arangodb::aql::AstNode const*,
                                      arangodb::aql::Variable const*,
                                      bool) override;

  bool allowExpansion() const override { return false; }

  bool canBeDropped() const override { return true; }

  bool isSorted() const override { return true; }

  bool hasSelectivityEstimate() const override { return false; }
  
  void toVelocyPack(VPackBuilder&, bool, bool) const override;
  // Uses default toVelocyPackFigures

  bool matchesDefinition(VPackSlice const& info) const override;

  void unload() override {}

  void truncate(transaction::Methods*) override;

  /// @brief looks up all points within a given radius
  /*arangodb::rocksdbengine::GeoCoordinates* withinQuery(transaction::Methods*,
                                                       double, double,
                                                       double) const;

  /// @brief looks up the nearest points
  arangodb::rocksdbengine::GeoCoordinates* nearQuery(transaction::Methods*,
                                                     double, double,
                                                     size_t) const;*/

  /*bool isSame(std::vector<std::string> const& location, bool geoJson) const {
    return (!_location.empty() && _location == location && _geoJson == geoJson);
  }

  bool isSame(std::vector<std::string> const& latitude,
              std::vector<std::string> const& longitude) const {
    return (!_latitude.empty() && !_longitude.empty() &&
            _latitude == latitude && _longitude == longitude);
  }*/

  /// insert index elements into the specified write batch.
  Result insertInternal(transaction::Methods* trx, RocksDBMethods*,
                        LocalDocumentId const& documentId,
                        arangodb::velocypack::Slice const&) override;

  /// remove index elements and put it in the specified write batch.
  Result removeInternal(transaction::Methods*, RocksDBMethods*, 
                        LocalDocumentId const& documentId,
                        arangodb::velocypack::Slice const&) override;

 private:
  /// internal insert function, set batch or trx before calling
  int internalInsert(LocalDocumentId const& documentId, velocypack::Slice const&);
  /// internal remove function, set batch or trx before calling
  int internalRemove(LocalDocumentId const& documentId, velocypack::Slice const&);

  /// @brief attribute paths
  std::vector<std::string> _location;
  std::vector<std::string> _latitude;
  std::vector<std::string> _longitude;

  /// @brief the geo index variant (geo1 or geo2)
  IndexVariant _variant;

  /// @brief whether the index is a geoJson index (latitude / longitude
  /// reversed)
  bool _geoJson;
;
};
}  // namespace rocksdbengine
}  // namespace arangodb


#endif
