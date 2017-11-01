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

#ifndef ARANGOD_GEO_GEOHELPER_H
#define ARANGOD_GEO_GEOHELPER_H 1

#include "Basics/Result.h"
#include <vector>
#include <velocypack/Slice.h>
#include <cstdint>
#include <geometry/s2cellid.h>

class S2RegionCoverer;

namespace arangodb {
namespace geo {

/// helper class to deal with S2RegionCoverer and S2Cell
class GeoHelper {
  private:
    GeoHelper() {}

  public:
  
  /// parses geojson or [lat, lng] pairs and turns them into
  /// a set of cellIds ready for indexing
  static Result generateS2CellIds(S2RegionCoverer* coverer,
                                  arangodb::velocypack::Slice const& field,
                                  bool isGeoJson,
                                  std::vector<S2CellId>& cells);
  
  /// convert lat, lng pair into cell id. Always uses max level
  static Result generateS2CellIdFromLatLng(double lat, double lng,
                                           std::vector<S2CellId>& cells);
  
  static bool isGeoJsonWithArea(arangodb::velocypack::Slice const& geoJson);
  
};

}  // namespace gep
}  // namespace arangodb

#endif
