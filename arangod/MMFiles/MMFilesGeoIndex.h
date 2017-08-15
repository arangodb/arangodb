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

#include "Basics/Common.h"
#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "MMFiles/mmfiles-geo-index.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

// GeoCoordinate.data must be capable of storing revision ids
static_assert(sizeof(GeoCoordinate::data) >= sizeof(TRI_voc_rid_t),
              "invalid size of GeoCoordinate.data");

namespace arangodb {
class MMFilesGeoIndex;

class MMFilesGeoIndexIterator final : public IndexIterator {
 public:
  /// @brief Construct an MMFilesGeoIndexIterator based on Ast Conditions
  MMFilesGeoIndexIterator(LogicalCollection* collection,
                          transaction::Methods* trx,
                          ManagedDocumentResult* mmdr,
                          MMFilesGeoIndex const* index,
                          arangodb::aql::AstNode const*,
                          arangodb::aql::Variable const*);

  ~MMFilesGeoIndexIterator() { replaceCursor(nullptr); }

  char const* typeName() const override { return "geo-index-iterator"; }

  bool next(TokenCallback const& cb, size_t limit) override;

  void reset() override;

 private:
  size_t findLastIndex(GeoCoordinates* coords) const;
  void replaceCursor(::GeoCursor* c);
  void createCursor(double lat, double lon);
  void evaluateCondition();  // called in constructor

  MMFilesGeoIndex const* _index;
  ::GeoCursor* _cursor;
  ::GeoCoordinate _coor;
  arangodb::aql::AstNode const* _condition;
  double _lat;
  double _lon;
  bool _near;
  bool _inclusive;
  bool _done;
  double _radius;
};

class MMFilesGeoIndex final : public Index {
  friend class MMFilesGeoIndexIterator;

 public:
  MMFilesGeoIndex() = delete;

  MMFilesGeoIndex(TRI_idx_iid_t, LogicalCollection*,
                  arangodb::velocypack::Slice const&);

  ~MMFilesGeoIndex();

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

  size_t memory() const override;

  void toVelocyPack(VPackBuilder&, bool withFigures, bool forPersistence) const override;
  // Uses default toVelocyPackFigures

  bool matchesDefinition(VPackSlice const& info) const override;

  Result insert(transaction::Methods*, TRI_voc_rid_t,
                arangodb::velocypack::Slice const&, bool isRollback) override;

  Result remove(transaction::Methods*, TRI_voc_rid_t,
                arangodb::velocypack::Slice const&, bool isRollback) override;

  void load() override {}
  void unload() override;

  /// @brief looks up all points within a given radius
  GeoCoordinates* withinQuery(transaction::Methods*, double, double,
                              double) const;

  /// @brief looks up the nearest points
  GeoCoordinates* nearQuery(transaction::Methods*, double, double,
                            size_t) const;

  bool isSame(std::vector<std::string> const& location, bool geoJson) const {
    return (!_location.empty() && _location == location && _geoJson == geoJson);
  }

  bool isSame(std::vector<std::string> const& latitude,
              std::vector<std::string> const& longitude) const {
    return (!_latitude.empty() && !_longitude.empty() &&
            _latitude == latitude && _longitude == longitude);
  }

  static uint64_t fromDocumentIdentifierToken(
      DocumentIdentifierToken const& token);

  static DocumentIdentifierToken toDocumentIdentifierToken(uint64_t internal);

 private:
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
  GeoIdx* _geoIndex;
};
}

namespace std {
template <>
class default_delete<GeoCoordinates> {
 public:
  void operator()(GeoCoordinates* result) {
    if (result != nullptr) {
      GeoIndex_CoordinatesFree(result);
    }
  }
};
}

#endif
