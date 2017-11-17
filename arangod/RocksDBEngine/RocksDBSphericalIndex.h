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

#ifndef ARANGOD_ROCKSDB_SPHERICAL_INDEX_H
#define ARANGOD_ROCKSDB_SPHERICAL_INDEX_H 1

#include "Basics/Result.h"
#include "Geo/GeoCover.h"
#include "Geo/GeoParams.h"
#include "Geo/Near.h"
#include "Indexes/IndexIterator.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "VocBase/voc-types.h"

#include <geometry/s2cellid.h>
#include <velocypack/Builder.h>

class S2Region;

namespace arangodb {
class RocksDBSphericalIndex;

/// Common spherical Iterator supertype
class RocksDBSphericalIndexIterator : public IndexIterator {
 public:
  RocksDBSphericalIndexIterator(LogicalCollection* collection,
                                transaction::Methods* trx,
                                ManagedDocumentResult* mmdr,
                                RocksDBSphericalIndex const* index);

  char const* typeName() const override { return "geospatial-index-iterator"; }

  virtual geo::FilterType filterType() const = 0;

 protected:
  RocksDBSphericalIndex const* _index;
  std::unique_ptr<rocksdb::Iterator> _iter;
};

class RocksDBSphericalIndex final : public arangodb::RocksDBIndex {
  friend class RocksDBSphericalIndexIterator;

 public:
  RocksDBSphericalIndex() = delete;

  RocksDBSphericalIndex(TRI_idx_iid_t, arangodb::LogicalCollection*,
                        velocypack::Slice const&);

  ~RocksDBSphericalIndex() override {}

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

 public:
  IndexType type() const override { return TRI_IDX_TYPE_GEOSPATIAL_INDEX; }

  char const* typeName() const override { return "geospatial"; }

  IndexIterator* iteratorForCondition(transaction::Methods*,
                                      ManagedDocumentResult*,
                                      arangodb::aql::AstNode const*,
                                      arangodb::aql::Variable const*,
                                      bool) override;

  bool allowExpansion() const override { return false; }

  bool canBeDropped() const override { return true; }

  bool isSorted() const override { return true; }

  bool hasSelectivityEstimate() const override { return false; }

  void toVelocyPack(velocypack::Builder&, bool, bool) const override;
  // Uses default toVelocyPackFigures

  bool matchesDefinition(velocypack::Slice const& info) const override;

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
                        arangodb::velocypack::Slice const&,
                        OperationMode mode) override;

  /// remove index elements and put it in the specified write batch.
  Result removeInternal(transaction::Methods*, RocksDBMethods*,
                        LocalDocumentId const& documentId,
                        arangodb::velocypack::Slice const&,
                        OperationMode mode) override;

  IndexVariant variant() const { return _variant; }

 private:
  Result parse(velocypack::Slice const& doc, std::vector<S2CellId>& cells,
               geo::Coordinate& co) const;

 private:
  /// @brief immutable region coverer parameters
  geo::RegionCoverParams _coverParams;

  /// @brief the type of geo we support
  IndexVariant _variant;

  /// @brief attribute paths
  std::vector<std::string> _location;
  std::vector<std::string> _latitude;
  std::vector<std::string> _longitude;
  ;
};
}  // namespace arangodb

#endif
