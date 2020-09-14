////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_GEO_NEAR_QUERY_H
#define ARANGOD_GEO_NEAR_QUERY_H 1

#include <queue>
#include <type_traits>
#include <vector>

#include <s2/s2cap.h>
#include <s2/s2cell_id.h>
#include <s2/s2region.h>
#include <s2/s2region_coverer.h>

#include "Geo/GeoParams.h"
#include "Geo/Utils.h"
#include "VocBase/Identifiers/LocalDocumentId.h"

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack
namespace geo_index {

/// result of a geospatial index query. distance may or may not be set
struct Document {
  Document(LocalDocumentId d, S1ChordAngle angle)
      : token(d), distAngle(angle) {}
  /// @brief LocalDocumentId
  LocalDocumentId token;
  /// @brief distance from centroids on the unit sphere
  S1ChordAngle distAngle;
};

struct DocumentsAscending {
  bool operator()(Document const& a, Document const& b) {
    return a.distAngle > b.distAngle;
  }
};

struct DocumentsDescending {
  bool operator()(Document const& a, Document const& b) {
    return a.distAngle < b.distAngle;
  }
};

/// @brief Helper class to build a simple near query iterator.
/// Will return points sorted by distance to the target point, can
/// also filter contains / intersect in regions (on result points and
/// search intervals). Should be storage engine agnostic
template <typename CMP = DocumentsAscending>
class NearUtils {
  static_assert(std::is_same<CMP, DocumentsAscending>::value ||
                    std::is_same<CMP, DocumentsDescending>::value,
                "Invalid template type");
  static constexpr bool isAscending() {
    return std::is_same<CMP, DocumentsAscending>::value;
  }
  static constexpr bool isDescending() {
    return std::is_same<CMP, DocumentsDescending>::value;
  }

 public:
  /// @brief Type of documents buffer
  typedef std::priority_queue<Document, std::vector<Document>, CMP> GeoDocumentsQueue;

  explicit NearUtils(geo::QueryParams&& params) noexcept;
  ~NearUtils();

 public:
  /// @brief get cell covering target coordinate (at max level)
  inline S2Point origin() const { return _origin; }

  inline geo::FilterType filterType() const { return _params.filterType; }

  inline geo::ShapeContainer const& filterShape() const {
    return _params.filterShape;
  }

  /// @brief all intervals are covered, no more buffered results
  bool isDone() const {
    TRI_ASSERT(_innerAngle >= S1ChordAngle::Zero() && _innerAngle <= _outerAngle);
    TRI_ASSERT(_outerAngle <= _maxAngle &&
               _maxAngle <= S1ChordAngle::Radians(geo::kMaxRadiansBetweenPoints));
    return _buffer.empty() && _allIntervalsCovered;
  }

  /// @brief has buffered results
  inline bool hasNearest() const {
    if (_allIntervalsCovered) {  // special case when almost done
      return !_buffer.empty();
    }
    // we need to not return results in the search area
    // between _innerAngle and _maxAngle. Otherwise results may appear
    // too early in the result list
    return !_buffer.empty() &&
           ((isAscending() && _buffer.top().distAngle <= _innerAngle) ||
            (isDescending() && _buffer.top().distAngle >= _outerAngle));
  }

  /// @brief closest buffered result
  geo_index::Document const& nearest() const {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    if (!_allIntervalsCovered) {
      TRI_ASSERT(!isAscending() || isFilterIntersects() ||
                 _buffer.top().distAngle <= _innerAngle);
      TRI_ASSERT(!isDescending() || _buffer.top().distAngle >= _outerAngle);
    }
#endif
    return _buffer.top();
  }

  /// @brief remove closest buffered result
  void popNearest() { _buffer.pop(); }

  /// @brief reset query to initial state
  void reset();

  /// aid density estimation by reporting a result close
  /// to the target coordinates
  void estimateDensity(S2Point const& found);

  /// Call only when current scan intervals contain no more results
  /// will internall track already returned intervals and not return
  /// new ones without calling updateBounds
  std::vector<geo::Interval> intervals();

  /// buffer and sort results
  void reportFound(LocalDocumentId lid, S2Point const& center);

  /// Call after scanning all intervals
  void didScanIntervals();

  /// @brief Reference to parameters used
  geo::QueryParams const& params() const { return _params; }

  size_t _found = 0;
  size_t _rejection = 0;

 private:
  /// @brief adjust the bounds delta
  void estimateDelta();
  /// @brief calculate the scan bounds
  void calculateBounds();

  /// @brief make isDone return true
  void invalidate() {
    _innerAngle = _maxAngle;
    _outerAngle = _maxAngle;
  }

  inline bool isFilterNone() const noexcept {
    return _params.filterType == geo::FilterType::NONE;
  }

  inline bool isFilterContains() const noexcept {
    return _params.filterType == geo::FilterType::CONTAINS;
  }

  inline bool isFilterIntersects() const noexcept {
    return _params.filterType == geo::FilterType::INTERSECTS;
  }

 private:
  geo::QueryParams const _params;

  /// target from which distances are measured
  S2Point const _origin;

  /// min distance in radians on the unit sphere
  S1ChordAngle const _minAngle = S1ChordAngle::Zero();
  /// max distance in radians on the unit sphere
  S1ChordAngle const _maxAngle = S1ChordAngle::Straight();

  /// Are all intervals covered
  bool _allIntervalsCovered;
  /// for adjusting _deltaAngle on the fly
  size_t _statsFoundLastInterval;
  /// Total number of interval calculations
  size_t _numScans;

  /// Amount to increment by
  S1ChordAngle _deltaAngle;
  /// inner limit of search area
  S1ChordAngle _innerAngle;
  /// outer limit of search area
  S1ChordAngle _outerAngle;

  /// buffer of found documents
  GeoDocumentsQueue _buffer;

  // deduplication filter
  std::unordered_set<uint64_t> _seenDocs;

  /// Track the already scanned region
  std::vector<S2CellId> _scannedCells;
  /// Coverer instance to use
  S2RegionCoverer _coverer;
};

}  // namespace geo_index
}  // namespace arangodb

#endif
