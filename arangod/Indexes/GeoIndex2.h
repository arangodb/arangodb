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

#ifndef ARANGOD_INDEXES_GEO_INDEX2_H
#define ARANGOD_INDEXES_GEO_INDEX2_H 1

#include "Basics/Common.h"
#include "GeoIndex/GeoIndex.h"
#include "Indexes/Index.h"
#include "VocBase/shaped-json.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-types.h"

class VocShaper;

namespace arangodb {

class GeoIndex2 final : public Index {
 public:
  GeoIndex2() = delete;

  GeoIndex2(TRI_idx_iid_t, struct TRI_document_collection_t*,
            std::vector<std::vector<arangodb::basics::AttributeName>> const&,
            std::vector<TRI_shape_pid_t> const&, bool);

  GeoIndex2(TRI_idx_iid_t, struct TRI_document_collection_t*,
            std::vector<std::vector<arangodb::basics::AttributeName>> const&,
            std::vector<TRI_shape_pid_t> const&);

  ~GeoIndex2();

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief geo index variants
  //////////////////////////////////////////////////////////////////////////////

  enum IndexVariant {
    INDEX_GEO_NONE = 0,
    INDEX_GEO_INDIVIDUAL_LAT_LON,
    INDEX_GEO_COMBINED_LAT_LON,
    INDEX_GEO_COMBINED_LON_LAT
  };

 public:
  IndexType type() const override final {
    if (_variant == INDEX_GEO_COMBINED_LAT_LON ||
        _variant == INDEX_GEO_COMBINED_LON_LAT) {
      return TRI_IDX_TYPE_GEO1_INDEX;
    }

    return TRI_IDX_TYPE_GEO2_INDEX;
  }

  bool isSorted() const override final { return false; }

  bool hasSelectivityEstimate() const override final { return false; }

  bool dumpFields() const override final { return true; }

  size_t memory() const override final;

  void toVelocyPack(VPackBuilder&, bool) const override final;
  // Uses default toVelocyPackFigures

  int insert(arangodb::Transaction*, struct TRI_doc_mptr_t const*,
             bool) override final;

  int remove(arangodb::Transaction*, struct TRI_doc_mptr_t const*,
             bool) override final;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief looks up all points within a given radius
  //////////////////////////////////////////////////////////////////////////////

  GeoCoordinates* withinQuery(arangodb::Transaction*, double, double,
                              double) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief looks up the nearest points
  //////////////////////////////////////////////////////////////////////////////

  GeoCoordinates* nearQuery(arangodb::Transaction*, double, double,
                            size_t) const;

  bool isSame(TRI_shape_pid_t location, bool geoJson) const {
    return (_location != 0 && _location == location && _geoJson == geoJson);
  }

  bool isSame(TRI_shape_pid_t latitude, TRI_shape_pid_t longitude) const {
    return (_latitude != 0 && _longitude != 0 && _latitude == latitude &&
            _longitude == longitude);
  }

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief extracts a double value from an object
  //////////////////////////////////////////////////////////////////////////////

  bool extractDoubleObject(VocShaper*, struct TRI_shaped_json_s const*, int,
                           double*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief extracts a double value from an array
  //////////////////////////////////////////////////////////////////////////////

  bool extractDoubleArray(VocShaper*, struct TRI_shaped_json_s const*, double*,
                          double*);

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief the attribute paths
  //////////////////////////////////////////////////////////////////////////////

  std::vector<TRI_shape_pid_t> const _paths;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief attribute paths
  //////////////////////////////////////////////////////////////////////////////

  TRI_shape_pid_t _location;
  TRI_shape_pid_t _latitude;
  TRI_shape_pid_t _longitude;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the geo index variant (geo1 or geo2)
  //////////////////////////////////////////////////////////////////////////////

  IndexVariant const _variant;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether the index is a geoJson index (latitude / longitude
  /// reversed)
  //////////////////////////////////////////////////////////////////////////////

  bool _geoJson;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the actual geo index
  //////////////////////////////////////////////////////////////////////////////

  GeoIndex* _geoIndex;
};
}

#endif
