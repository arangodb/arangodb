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
#include "Geo/GeoCover.h"

#include <queue>
#include <vector>
#include <geometry/s2cellid.h>
#include <geometry/s2cellunion.h>
#include <geometry/s2cap.h>
#include <geometry/s2regioncoverer.h>

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}
namespace geo {

/// @brief helper class to build a simple near query iterator.
/// Main design goal is to be modular and storage engine agnostic
class NearQuery {
  
public:
  
  typedef std::priority_queue<GeoDocument, std::vector<GeoDocument>,
                              GeoDocumentCompare> GeoDocumentsQueue;
  
  NearQuery(NearQueryParams const& params);
  
public:
  
  bool hasNearest() const { return !_buffer.empty(); }
  GeoDocument const& nearest() const { return _buffer.top(); }
  void popNearest() { _buffer.pop(); }
  
  /// Call only once current intervals contain
  /// no more results
  std::vector<GeoCover::Interval> intervals();
  
  void reportFound(GeoDocument&& doc);
  
  bool done() const;

private:
  geo::NearQueryParams const _params;
  S2Point const _centroid;
  
  /// buffer of found documents
  GeoDocumentsQueue _buffer;
  // for deduplication
  std::unordered_map<TRI_voc_rid_t, double> _seen;
  
  // Amount to increment by (in length on unit sphere)
  double _boundDelta;
  //double _currentInnerBound;
  // inner limit (in length on unit sphere)
  double _innerBound;
  // outer limit (in length on unit sphere)
  double _outerBound;
  double _maxBounds;
  
  size_t _numFoundLastInterval;
  /// Track the already scanned region
  S2CellUnion _scannedCells;
  /// full area to scan
  //S2Cap _fullScanBounds;
  S2RegionCoverer _coverer;
};

}  // namespace geo
}  // namespace arangodb

#endif
