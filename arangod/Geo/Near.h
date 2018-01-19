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

#ifndef ARANGOD_GEO_NEAR_QUERY_H
#define ARANGOD_GEO_NEAR_QUERY_H 1

#include "Geo/GeoParams.h"
#include "Geo/GeoUtils.h"
#include "VocBase/LocalDocumentId.h"

#include <geometry/s2cap.h>
#include <geometry/s2cellid.h>
#include <geometry/s2cellunion.h>
#include <geometry/s2region.h>
#include <geometry/s2regioncoverer.h>
#include <queue>
#include <type_traits>
#include <vector>

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}
namespace geo {

/// result of a geospatial index query. distance may or may not be set
struct Document {
  Document(LocalDocumentId d, double rad) : document(d), distRad(rad) {}
  /// @brief LocalDocumentId
  LocalDocumentId document;
  /// @brief distance from centroids on the unit sphere
  double distRad;
};

struct DocumentsAscending {
  bool operator()(Document const& a, Document const& b) {
    return a.distRad > b.distRad;
  }
};

struct DocumentsDescending {
  bool operator()(Document const& a, Document const& b) {
    return a.distRad < b.distRad;
  }
};

/// @brief Helper class to build a simple near query iterator.
/// Will return points sorted by distance to the target point, can
/// also filter contains / intersect in regions (on rsesult points and
/// search intervals). Should be storage engine agnostic
template <typename CMP = DocumentsAscending>
class NearUtils {
  static_assert(std::is_same<CMP, DocumentsAscending>::value ||
                    std::is_same<CMP, DocumentsDescending>::value,
                "Invalid template type");
  static constexpr bool isAcending() {
    return std::is_same<CMP, DocumentsAscending>::value;
  }
  static constexpr bool isDescending() {
    return std::is_same<CMP, DocumentsDescending>::value;
  }

 public:
  /// @brief Type of documents buffer
  typedef std::priority_queue<Document, std::vector<Document>, CMP>
      GeoDocumentsQueue;

  NearUtils(geo::QueryParams&& params);

 public:
  /// @brief get cell covering target coordinate (at max level)
  S2Point origin() const { return _origin; }

  geo::FilterType filterType() const { return _params.filterType; }

  geo::ShapeContainer const& filterShape() const { return _params.filterShape; }
  
  /// @brief all intervals are covered, no more buffered results
  bool isDone() const {
    TRI_ASSERT(_innerBound >= 0 && _innerBound <= _outerBound);
    TRI_ASSERT(_outerBound <= _maxBound && _maxBound <= M_PI);
    return _buffer.empty() && allIntervalsCovered();
  }

  /// @brief has buffered results
  inline bool hasNearest() const {
    if (allIntervalsCovered()) { // special case when almost done
      return !_buffer.empty();
    }
    // we need to not return results in the search area
    // between _innerBound and _maxBound. Otherwise results may appear
    // too early in the result list
    return !_buffer.empty() &&
           ((isAcending() && _buffer.top().distRad <= _innerBound) ||
            (isDescending() && _buffer.top().distRad >= _outerBound));
  }

  /// @brief closest buffered result
  geo::Document const& nearest() const {
    TRI_ASSERT(isAcending() && (isFilterIntersects() ||
                                _buffer.top().distRad <= _innerBound) ||
               isDescending() && _buffer.top().distRad >= _outerBound);
    return _buffer.top();
  }

  /// @brief remove closest buffered result
  void popNearest() { _buffer.pop(); }

  /// @brief reset query to inital state
  void reset();

  /// Call only when current scan intervals contain no more results
  /// will internall track already returned intervals and not return
  /// new ones without calling updateBounds
  std::vector<geo::Interval> intervals();

  /// buffer and sort results
  void reportFound(LocalDocumentId lid, geo::Coordinate const& center);

  /// aid density estimation by reporting a result close
  /// to the target coordinates
  void estimateDensity(geo::Coordinate const& found);

 private:
  /// @brief adjust the bounds delta
  void estimateDelta();

  /// @brief make isDone return true
  void invalidate() {
    _innerBound = _maxBound;
    _outerBound = _maxBound;
  }
  
  /// @brief returns true if all possible scan intervals are covered
  inline bool allIntervalsCovered() const noexcept {
    return (isAcending() && _innerBound == _maxBound && _outerBound == _maxBound) ||
            (isDescending() && _innerBound == 0 && _outerBound == 0);
  }
  
  inline bool isFilterNone() const noexcept {
    return _params.filterType == FilterType::NONE;
  }
  
  inline bool isFilterContains() const noexcept {
    return _params.filterType == FilterType::CONTAINS;
  }
  
  inline bool isFilterIntersects() const noexcept {
    return _params.filterType == FilterType::INTERSECTS;
  }

 private:
  geo::QueryParams const _params;

  /// target from which distances are measured
  S2Point const _origin;

  /// min distance on the unit spherer or <M_PI
  double const _minBound = 0;
  /// max distance on the unit spherer or M_PI
  double const _maxBound = M_PI;

  /// Amount to increment by (in radians on unit sphere)
  double _boundDelta = 0.0;
  /// inner limit (in radians on unit sphere) of search area
  double _innerBound = 0.0;
  /// outer limit (in radians on unit sphere) of search area
  double _outerBound = 0.0;

  /// for adjusting _boundDelta on the fly
  size_t _statsFoundLastInterval = 0;

  /// buffer of found documents
  GeoDocumentsQueue _buffer;

  /// for deduplication of results
  std::unordered_map<LocalDocumentId, double> _seen;

  /// Track the already scanned region
  S2CellUnion _scannedCells;
  /// Coverer instance to use
  S2RegionCoverer _coverer;
};

}  // namespace geo
}  // namespace arangodb

#endif
