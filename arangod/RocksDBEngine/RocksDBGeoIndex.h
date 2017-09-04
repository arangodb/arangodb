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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_MMFILES_GEO_INDEX_H
#define ARANGOD_MMFILES_GEO_INDEX_H 1

#include "Basics/Common.h"
#include "Indexes/IndexIterator.h"
#include "RocksDBEngine/RocksDBGeoIndexImpl.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>
#include <type_traits>

namespace arangodb {

// GeoCoordinate.data must be capable of storing revision ids
static_assert(sizeof(arangodb::rocksdbengine::GeoCoordinate::data) >=
                  sizeof(TRI_voc_rid_t),
              "invalid size of GeoCoordinate.data");

class RocksDBGeoIndex;
class RocksDBGeoIndexIterator final : public IndexIterator {
 public:
  /// @brief Construct an RocksDBGeoIndexIterator based on Ast Conditions
  RocksDBGeoIndexIterator(LogicalCollection* collection,
                          transaction::Methods* trx,
                          ManagedDocumentResult* mmdr,
                          RocksDBGeoIndex const* index,
                          arangodb::aql::AstNode const*,
                          arangodb::aql::Variable const*);

  ~RocksDBGeoIndexIterator() { replaceCursor(nullptr); }

  char const* typeName() const override { return "geo-index-iterator"; }

  bool next(TokenCallback const& cb, size_t limit) override;

  void reset() override;

 private:
  size_t findLastIndex(arangodb::rocksdbengine::GeoCoordinates* coords) const;
  void replaceCursor(arangodb::rocksdbengine::GeoCursor* c);
  void createCursor(double lat, double lon);
  void evaluateCondition();  // called in constructor

  RocksDBGeoIndex const* _index;
  arangodb::rocksdbengine::GeoCursor* _cursor;
  arangodb::rocksdbengine::GeoCoordinate _coor;
  arangodb::aql::AstNode const* _condition;
  double _lat;
  double _lon;
  bool _near;
  bool _inclusive;
  bool _done;
  double _radius;
};

class RocksDBGeoIndex final : public RocksDBIndex {
  friend class RocksDBGeoIndexIterator;

 public:
  RocksDBGeoIndex() = delete;

  RocksDBGeoIndex(TRI_idx_iid_t, LogicalCollection*,
                  arangodb::velocypack::Slice const&);

  ~RocksDBGeoIndex();

 public:
  /// @brief geo index variants
  enum IndexVariant {
    INDEX_GEO_NONE = 0,
    INDEX_GEO_INDIVIDUAL_LAT_LON,
    INDEX_GEO_COMBINED_LAT_LON,
    INDEX_GEO_COMBINED_LON_LAT
  };

 public:
  IndexType type() const override {
    if (_variant == INDEX_GEO_COMBINED_LAT_LON ||
        _variant == INDEX_GEO_COMBINED_LON_LAT) {
      return TRI_IDX_TYPE_GEO1_INDEX;
    }

    return TRI_IDX_TYPE_GEO2_INDEX;
  }

  char const* typeName() const override {
    if (_variant == INDEX_GEO_COMBINED_LAT_LON ||
        _variant == INDEX_GEO_COMBINED_LON_LAT) {
      return "geo1";
    }
    return "geo2";
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
  arangodb::rocksdbengine::GeoCoordinates* withinQuery(transaction::Methods*,
                                                       double, double,
                                                       double) const;

  /// @brief looks up the nearest points
  arangodb::rocksdbengine::GeoCoordinates* nearQuery(transaction::Methods*,
                                                     double, double,
                                                     size_t) const;

  bool isSame(std::vector<std::string> const& location, bool geoJson) const {
    return (!_location.empty() && _location == location && _geoJson == geoJson);
  }

  bool isSame(std::vector<std::string> const& latitude,
              std::vector<std::string> const& longitude) const {
    return (!_latitude.empty() && !_longitude.empty() &&
            _latitude == latitude && _longitude == longitude);
  }

  /// insert index elements into the specified write batch.
  Result insertInternal(transaction::Methods* trx, RocksDBMethods*,
                        TRI_voc_rid_t,
                        arangodb::velocypack::Slice const&) override;

  /// remove index elements and put it in the specified write batch.
  Result removeInternal(transaction::Methods*, RocksDBMethods*, TRI_voc_rid_t,
                        arangodb::velocypack::Slice const&) override;

 private:
  /// internal insert function, set batch or trx before calling
  int internalInsert(TRI_voc_rid_t, velocypack::Slice const&);
  /// internal remove function, set batch or trx before calling
  int internalRemove(TRI_voc_rid_t, velocypack::Slice const&);

  /// @brief attribute paths
  std::vector<std::string> _location;
  std::vector<std::string> _latitude;
  std::vector<std::string> _longitude;

  /// @brief the geo index variant (geo1 or geo2)
  IndexVariant _variant;

  /// @brief whether the index is a geoJson index (latitude / longitude
  /// reversed)
  bool _geoJson;

  /// @brief the actual geo index
  arangodb::rocksdbengine::GeoIdx* _geoIndex;
};
}  // namespace arangodb

namespace std {
template <>
class default_delete<arangodb::rocksdbengine::GeoCoordinates> {
 public:
  void operator()(arangodb::rocksdbengine::GeoCoordinates* result) {
    if (result != nullptr) {
      GeoIndex_CoordinatesFree(result);
    }
  }
};
}  // namespace std

#endif
