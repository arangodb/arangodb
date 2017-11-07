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

#include <queue>
#include <vector>
#include <geometry/s2cellid.h>
#include <geometry/s2cellunion.h>
#include <geometry/s2cap.h>

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}
namespace geo {

/// @brief helper class to build a simple near query iterator.
/// Main design goal is to be modular and storage engine agnostic
class NearQueryUtils {
  
public:
  
  typedef std::priority_queue<GeoDocument, std::vector<GeoDocument>,
                              GeoDocumentCompare> GeoDocumentsQueue;
  
  NearQueryUtils(NearQueryParams const& params);
  
public:
  
  bool isDone() const;
  
  std::vector<S2CellId> scanIntervalls() const;
  
  void reportFound(GeoDocument&& doc) {
    _documents.push(doc);
  }
  
  GeoDocument const& nearest() const { return _documents.top(); }
  
  void popNearest() { _documents.pop(); }


private:
  geo::NearQueryParams const _params;
  /// priority queue of found documents
  GeoDocumentsQueue _documents;
  
  // Amount to increment the next bounds by
  double _boundsIncrement;
  double _currentInnerBound;
  double _currentOuterBound;
  
  /// Track the already scanned region
  S2CellUnion _scannedCells;
  /// full area to scan
  S2Cap _fullScanBounds;
  /// area scanned atm
  //S2RegionUnion _currentBounds;
};

}  // namespace geo
}  // namespace arangodb

#endif
