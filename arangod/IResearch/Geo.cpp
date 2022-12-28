////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "IResearch/Geo.h"
#include "IResearch/IResearchCommon.h"
#include "Geo/GeoJson.h"
#include "Geo/ShapeContainer.h"
#include "Logger/LogMacros.h"

#include <s2/s2latlng.h>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb::iresearch {

template<Parsing p>
bool parseShape(velocypack::Slice vpack, geo::ShapeContainer& region,
                std::vector<S2Point>& cache) {
  Result r;
  if (vpack.isArray()) {
    r = geo::json::parseCoordinates<p != Parsing::FromIndex>(vpack, region,
                                                             /*geoJson=*/true);
  } else if constexpr (p == Parsing::OnlyPoint) {
    S2LatLng latLng;
    r = geo::json::parsePoint(vpack, latLng);
    if (ADB_LIKELY(r.ok())) {
      region.reset(latLng.ToPoint());
    }
  } else {
    r = geo::json::parseRegion<p != Parsing::FromIndex>(vpack, region, cache);
  }
  if (p != Parsing::FromIndex && r.fail()) {
    LOG_TOPIC("4549c", DEBUG, TOPIC)
        << "Failed to parse value as GEO JSON or array of coordinates, error '"
        << r.errorMessage() << "'";
    return false;
  }
  return true;
}

template bool parseShape<Parsing::FromIndex>(velocypack::Slice slice,
                                             geo::ShapeContainer& shape,
                                             std::vector<S2Point>& cache);
template bool parseShape<Parsing::OnlyPoint>(velocypack::Slice slice,
                                             geo::ShapeContainer& shape,
                                             std::vector<S2Point>& cache);
template bool parseShape<Parsing::GeoJson>(velocypack::Slice slice,
                                           geo::ShapeContainer& shape,
                                           std::vector<S2Point>& cache);

void toVelocyPack(velocypack::Builder& builder, S2LatLng const& latLng) {
  builder.openArray();
  builder.add(velocypack::Value(latLng.lng().degrees()));
  builder.add(velocypack::Value(latLng.lat().degrees()));
  builder.close();
}

}  // namespace arangodb::iresearch
