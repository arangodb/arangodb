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

#include "Geo/GeoCover.h"
#include "Geo/GeoParams.h"
#include "Geo/Shapes.h"

#include <geometry/s2cap.h>
#include <geometry/s2cellid.h>
#include <geometry/s2cellunion.h>
#include <geometry/s2regioncoverer.h>
#include <queue>
#include <vector>

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}
namespace geo {

struct NearQueryParams {
  NearQueryParams(geo::Coordinate center)
      : centroid(center),
        cover(queryMaxCoverCells, queryWorstLevel, queryBestLevel) {}

  /// Centroid from which to start
  Coordinate centroid;
  // Min and max distance from centroid that we're willing to search.
  double minDistance = 0.0;
  // entire earth (halfaround in each direction)
  double maxDistance = kEarthRadiusInMeters * M_PI;
  bool maxInclusive = true;

  // parameters to calculate the coverage
  RegionCoverParams cover;

  static constexpr int queryWorstLevel = 2;
  static constexpr int queryBestLevel = 23;  // about 1m
  static constexpr int queryMaxCoverCells = 20;
};

/// @brief result of a geospatial index query.
///        distance may or may not be set
struct GeoDocument {
  GeoDocument(TRI_voc_rid_t doc, double rad) : rid(doc), radians(doc) {}
  /// @brief LocalDocumentId
  TRI_voc_rid_t rid;
  /// @brief distance from centroids on the unit sphere
  double radians;
};

struct GeoDocumentCompare {
  bool operator()(GeoDocument const& a, GeoDocument const& b) {
    return a.radians < b.radians;
  }
};

/// @brief helper class to build a simple near query iterator.
/// Main design goal is to be modular and storage engine agnostic
class NearQuery {
 public:
  typedef std::priority_queue<GeoDocument, std::vector<GeoDocument>,
                              GeoDocumentCompare>
      GeoDocumentsQueue;

  NearQuery(NearQueryParams const& params);

 public:
  
  /// @brief get cell covering target coordinate (at max level)
  S2Point centroid() const {
    return _centroid;
  }
  
  /// @brief has buffered results
  bool hasNearest() const { return !_buffer.empty(); }
  
  /// @brief closest buffered result
  GeoDocument const& nearest() const { return _buffer.top(); }
  
  /// remove closest buffered result
  void popNearest() { _buffer.pop(); }

  /// @brief reset query to inital state
  void reset();

  /// Call only when current scan intervals contain no more results
  /// will internall track already returned intervals and not return
  /// new ones without calling updateBounds
  std::vector<GeoCover::Interval> intervals();

  /// @brief buffer and sort results
  void reportFound(TRI_voc_rid_t rid, geo::Coordinate const& center);
  
  /// @brief aid density estimation by reporting a result close
  /// to the target coordinates
  void estimateDensity(geo::Coordinate const& found);

  /// @brief all intervals are covered, no more buffered results
  bool isDone() const;
  
  /// @brief make isDone return true
  void invalidate();

 private:
  geo::NearQueryParams const _params;
  
  // target from which distances are measured
  S2Point const _centroid;

  // max distance on the unit spherer or M_PI
  double const _maxBounds = M_PI;

  // Amount to increment by (in radians on unit sphere)
  double _boundDelta = 0.0;

  // inner limit (in radians on unit sphere)
  double _lastInnerBound = 0.0;

  // inner limit (in radians on unit sphere)
  double _innerBound = 0.0;

  // outer limit (in radians on unit sphere)
  double _outerBound = 0.0;

  // for adjusting _boundDelta on the fly
  size_t _numFoundLastInterval = 0;
  
  /// buffer of found documents
  GeoDocumentsQueue _buffer;
  
  // for deduplication of results
  std::unordered_map<TRI_voc_rid_t, double> _seen;
  
  /// Track the already scanned region
  S2CellUnion _scannedCells;
  /// full area to scan
  // S2Cap _fullScanBounds;
  S2RegionCoverer _coverer;
};

}  // namespace geo
}  // namespace arangodb

#endif
