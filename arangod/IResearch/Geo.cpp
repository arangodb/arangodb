////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

template<bool Validation>
bool parseShape(velocypack::Slice vpack, geo::ShapeContainer& region,
                bool onlyPoint) {
  Result r;
  if (vpack.isObject()) {
    if (onlyPoint) {
      S2LatLng latLng;
      r = geo::json::parsePoint<Validation>(vpack, latLng);
      if (!Validation || r.ok()) {
        region.reset(latLng.ToPoint());
      }
    } else {
      r = geo::json::parseRegion<Validation>(vpack, region,
                                             /*legacy=*/false);
    }
  } else if (vpack.isArray()) {
    r = geo::json::parseCoordinates<Validation>(vpack, region,
                                                /*geoJson=*/true);
  } else {
    LOG_TOPIC("4449c", DEBUG, TOPIC)
        << "Geo JSON or array of coordinates expected, got '"
        << vpack.typeName() << "'";
    return false;
  }
  if (Validation && r.fail()) {
    LOG_TOPIC("4549c", DEBUG, TOPIC)
        << "Failed to parse value as GEO JSON or array of coordinates, error '"
        << r.errorMessage() << "'";

    return false;
  }
  return true;
}

template bool parseShape<true>(velocypack::Slice vpack,
                               geo::ShapeContainer& region, bool onlyPoint);
template bool parseShape<false>(velocypack::Slice vpack,
                                geo::ShapeContainer& region, bool onlyPoint);

void toVelocyPack(velocypack::Builder& builder, S2LatLng const& latLng) {
  builder.openArray();
  builder.add(velocypack::Value(latLng.lng().degrees()));
  builder.add(velocypack::Value(latLng.lat().degrees()));
  builder.close();
}

}  // namespace arangodb::iresearch
